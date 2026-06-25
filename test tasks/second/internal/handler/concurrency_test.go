package handler_test

import (
	"bytes"
	"context"
	"encoding/json"
	"log/slog"
	"net/http"
	"net/http/httptest"
	"os"
	"sync"
	"sync/atomic"
	"testing"
	"time"

	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
	"github.com/yourname/org-api/internal/apierr"
	"github.com/yourname/org-api/internal/dto"
	"github.com/yourname/org-api/internal/handler"
)

func newFullServer(svc *mockDepartmentService) *httptest.Server {
	logger := slog.New(slog.NewJSONHandler(os.Stdout, nil))
	deptH := handler.NewDepartmentHandler(svc)
	empH := handler.NewEmployeeHandler(&mockEmployeeService{})
	return httptest.NewServer(handler.NewRouter(logger, deptH, empH))
}

func TestConcurrent_UniqueRequestIDs(t *testing.T) {
	svc := &mockDepartmentService{
		createFn: func(_ context.Context, req *dto.CreateDepartmentRequest) (*dto.DepartmentResponse, error) {
			return &dto.DepartmentResponse{ID: 1, Name: req.Name}, nil
		},
	}

	server := newFullServer(svc)
	defer server.Close()

	const n = 50
	ids := make([]string, n)
	var wg sync.WaitGroup

	for i := 0; i < n; i++ {
		wg.Add(1)
		go func(idx int) {
			defer wg.Done()
			body, _ := json.Marshal(map[string]any{"name": "Engineering"})
			resp, err := http.Post(server.URL+"/departments/", "application/json", bytes.NewReader(body))
			if err != nil {
				return
			}
			defer resp.Body.Close()
			ids[idx] = resp.Header.Get("X-Request-ID")
		}(i)
	}
	wg.Wait()

	seen := make(map[string]bool)
	for _, id := range ids {
		require.NotEmpty(t, id, "X-Request-ID не должен быть пустым")
		assert.False(t, seen[id], "дубль X-Request-ID: %s", id)
		seen[id] = true
	}
}

func TestConcurrent_ParallelReadsNoRace(t *testing.T) {
	var callCount atomic.Int64

	svc := &mockDepartmentService{
		getFn: func(_ context.Context, id uint, _ int, _ bool, _ string) (*dto.DepartmentResponse, error) {
			callCount.Add(1)
			time.Sleep(time.Millisecond) // имитируем задержку БД
			return &dto.DepartmentResponse{ID: id, Name: "Engineering"}, nil
		},
	}

	server := newFullServer(svc)
	defer server.Close()

	const n = 100
	statuses := make([]int, n)
	var wg sync.WaitGroup

	for i := 0; i < n; i++ {
		wg.Add(1)
		go func(idx int) {
			defer wg.Done()
			resp, err := http.Get(server.URL + "/departments/1")
			if err != nil {
				return
			}
			defer resp.Body.Close()
			statuses[idx] = resp.StatusCode
		}(i)
	}
	wg.Wait()

	for i, code := range statuses {
		assert.Equal(t, http.StatusOK, code, "горутина %d: неожиданный статус", i)
	}
	assert.Equal(t, int64(n), callCount.Load(), "каждый запрос должен дойти до сервиса")
}

func TestConcurrent_DuplicateNameRace(t *testing.T) {
	var mu sync.Mutex
	created := false

	svc := &mockDepartmentService{
		createFn: func(_ context.Context, req *dto.CreateDepartmentRequest) (*dto.DepartmentResponse, error) {
			mu.Lock()
			defer mu.Unlock()
			if created {
				return nil, &apierr.ConflictError{Message: "department with this name already exists"}
			}
			created = true
			return &dto.DepartmentResponse{ID: 1, Name: req.Name}, nil
		},
	}

	server := newFullServer(svc)
	defer server.Close()

	const n = 30
	statuses := make([]int, n)
	var wg sync.WaitGroup

	for i := 0; i < n; i++ {
		wg.Add(1)
		go func(idx int) {
			defer wg.Done()
			body, _ := json.Marshal(map[string]any{"name": "Engineering"})
			resp, err := http.Post(server.URL+"/departments/", "application/json", bytes.NewReader(body))
			if err != nil {
				return
			}
			defer resp.Body.Close()
			statuses[idx] = resp.StatusCode
		}(i)
	}
	wg.Wait()

	created201, conflict409 := 0, 0
	for _, code := range statuses {
		switch code {
		case http.StatusCreated:
			created201++
		case http.StatusConflict:
			conflict409++
		}
	}

	assert.Equal(t, 1, created201, "ровно один запрос должен создать отдел")
	assert.Equal(t, n-1, conflict409, "остальные должны получить 409")
}

func TestConcurrent_PanicRecovery(t *testing.T) {
	var callCount atomic.Int64

	svc := &mockDepartmentService{
		getFn: func(_ context.Context, id uint, _ int, _ bool, _ string) (*dto.DepartmentResponse, error) {
			n := callCount.Add(1)
			if n%5 == 0 {
				panic("симулируем редкий panic в обработчике")
			}
			return &dto.DepartmentResponse{ID: id, Name: "Engineering"}, nil
		},
	}

	server := newFullServer(svc) 
	defer server.Close()

	const total = 25
	statuses := make([]int, total)
	var wg sync.WaitGroup

	for i := 0; i < total; i++ {
		wg.Add(1)
		go func(idx int) {
			defer wg.Done()
			resp, err := http.Get(server.URL + "/departments/1")
			if err != nil {
				statuses[idx] = -1
				return
			}
			defer resp.Body.Close()
			statuses[idx] = resp.StatusCode
		}(i)
	}
	wg.Wait()

	ok := 0
	recovered := 0
	for _, code := range statuses {
		switch code {
		case http.StatusOK:
			ok++
		case http.StatusInternalServerError:
			recovered++ 
		}
	}

	assert.Equal(t, total/5, recovered, "паники должны превращаться в 500")
	assert.Equal(t, total-total/5, ok, "остальные запросы должны проходить нормально")
	t.Logf("ok=%d, recovered_panics=%d", ok, recovered)
}

func TestConcurrent_DeleteAndCreate(t *testing.T) {
	svc := &mockDepartmentService{
		createFn: func(_ context.Context, req *dto.CreateDepartmentRequest) (*dto.DepartmentResponse, error) {
			time.Sleep(time.Millisecond)
			return &dto.DepartmentResponse{ID: 1, Name: req.Name}, nil
		},
		deleteFn: func(_ context.Context, id uint, mode string, _ *uint) error {
			time.Sleep(time.Millisecond)
			return nil
		},
	}

	server := newFullServer(svc)
	defer server.Close()

	var wg sync.WaitGroup
	const n = 20

	for i := 0; i < n; i++ {
		wg.Add(2)
		go func() {
			defer wg.Done()
			body, _ := json.Marshal(map[string]any{"name": "Engineering"})
			resp, _ := http.Post(server.URL+"/departments/", "application/json", bytes.NewReader(body))
			if resp != nil {
				resp.Body.Close()
			}
		}()
		go func() {
			defer wg.Done()
			req, _ := http.NewRequest(http.MethodDelete, server.URL+"/departments/1?mode=cascade", nil)
			resp, _ := http.DefaultClient.Do(req)
			if resp != nil {
				resp.Body.Close()
			}
		}()
	}
	wg.Wait()
}
