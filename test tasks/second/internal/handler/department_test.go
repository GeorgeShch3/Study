package handler_test

import (
	"bytes"
	"context"
	"encoding/json"
	"net/http"
	"net/http/httptest"
	"testing"
	"time"

	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
	"github.com/yourname/org-api/internal/apierr"
	"github.com/yourname/org-api/internal/dto"
	"github.com/yourname/org-api/internal/handler"
)


type mockDepartmentService struct {
	createFn func(ctx context.Context, req *dto.CreateDepartmentRequest) (*dto.DepartmentResponse, error)
	getFn    func(ctx context.Context, id uint, depth int, includeEmployees bool, sortBy string) (*dto.DepartmentResponse, error)
	updateFn func(ctx context.Context, id uint, req *dto.UpdateDepartmentRequest) (*dto.DepartmentResponse, error)
	deleteFn func(ctx context.Context, id uint, mode string, reassignTo *uint) error
}

func (m *mockDepartmentService) Create(ctx context.Context, req *dto.CreateDepartmentRequest) (*dto.DepartmentResponse, error) {
	return m.createFn(ctx, req)
}
func (m *mockDepartmentService) Get(ctx context.Context, id uint, depth int, includeEmployees bool, sortBy string) (*dto.DepartmentResponse, error) {
	return m.getFn(ctx, id, depth, includeEmployees, sortBy)
}
func (m *mockDepartmentService) Update(ctx context.Context, id uint, req *dto.UpdateDepartmentRequest) (*dto.DepartmentResponse, error) {
	return m.updateFn(ctx, id, req)
}
func (m *mockDepartmentService) Delete(ctx context.Context, id uint, mode string, reassignTo *uint) error {
	return m.deleteFn(ctx, id, mode, reassignTo)
}


func newTestRouter(svc *mockDepartmentService) http.Handler {
	deptH := handler.NewDepartmentHandler(svc)
	empH := handler.NewEmployeeHandler(&mockEmployeeService{})
	return handler.NewRouterNoMiddleware(deptH, empH)
}

func ptr[T any](v T) *T { return &v }


func TestCreateDepartment_Success(t *testing.T) {
	now := time.Now()
	svc := &mockDepartmentService{
		createFn: func(_ context.Context, req *dto.CreateDepartmentRequest) (*dto.DepartmentResponse, error) {
			return &dto.DepartmentResponse{
				ID:        1,
				Name:      req.Name,
				ParentID:  nil,
				CreatedAt: now,
			}, nil
		},
	}

	body, _ := json.Marshal(map[string]any{"name": "Engineering"})
	req := httptest.NewRequest(http.MethodPost, "/departments/", bytes.NewReader(body))
	req.Header.Set("Content-Type", "application/json")
	w := httptest.NewRecorder()

	newTestRouter(svc).ServeHTTP(w, req)

	assert.Equal(t, http.StatusCreated, w.Code)

	var resp dto.DepartmentResponse
	require.NoError(t, json.NewDecoder(w.Body).Decode(&resp))
	assert.Equal(t, uint(1), resp.ID)
	assert.Equal(t, "Engineering", resp.Name)
}

func TestCreateDepartment_InvalidBody(t *testing.T) {
	svc := &mockDepartmentService{}

	req := httptest.NewRequest(http.MethodPost, "/departments/", bytes.NewReader([]byte("not-json")))
	req.Header.Set("Content-Type", "application/json")
	w := httptest.NewRecorder()

	newTestRouter(svc).ServeHTTP(w, req)

	assert.Equal(t, http.StatusBadRequest, w.Code)
}

func TestCreateDepartment_ValidationError(t *testing.T) {
	svc := &mockDepartmentService{
		createFn: func(_ context.Context, req *dto.CreateDepartmentRequest) (*dto.DepartmentResponse, error) {
			return nil, &apierr.ValidationError{Message: "name must not be empty"}
		},
	}

	body, _ := json.Marshal(map[string]any{"name": ""})
	req := httptest.NewRequest(http.MethodPost, "/departments/", bytes.NewReader(body))
	req.Header.Set("Content-Type", "application/json")
	w := httptest.NewRecorder()

	newTestRouter(svc).ServeHTTP(w, req)

	assert.Equal(t, http.StatusBadRequest, w.Code)
}

func TestCreateDepartment_ConflictError(t *testing.T) {
	svc := &mockDepartmentService{
		createFn: func(_ context.Context, req *dto.CreateDepartmentRequest) (*dto.DepartmentResponse, error) {
			return nil, &apierr.ConflictError{Message: "already exists"}
		},
	}

	body, _ := json.Marshal(map[string]any{"name": "Engineering"})
	req := httptest.NewRequest(http.MethodPost, "/departments/", bytes.NewReader(body))
	req.Header.Set("Content-Type", "application/json")
	w := httptest.NewRecorder()

	newTestRouter(svc).ServeHTTP(w, req)

	assert.Equal(t, http.StatusConflict, w.Code)
}


func TestGetDepartment_Success(t *testing.T) {
	now := time.Now()
	svc := &mockDepartmentService{
		getFn: func(_ context.Context, id uint, depth int, includeEmployees bool, sortBy string) (*dto.DepartmentResponse, error) {
			assert.Equal(t, uint(42), id)
			assert.Equal(t, 2, depth)
			assert.True(t, includeEmployees)
			return &dto.DepartmentResponse{
				ID:        42,
				Name:      "Engineering",
				CreatedAt: now,
				Children: []*dto.DepartmentResponse{
					{ID: 2, Name: "Backend", CreatedAt: now},
				},
			}, nil
		},
	}

	req := httptest.NewRequest(http.MethodGet, "/departments/42?depth=2", nil)
	w := httptest.NewRecorder()

	newTestRouter(svc).ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code)

	var resp dto.DepartmentResponse
	require.NoError(t, json.NewDecoder(w.Body).Decode(&resp))
	assert.Equal(t, uint(42), resp.ID)
	assert.Len(t, resp.Children, 1)
}

func TestGetDepartment_NotFound(t *testing.T) {
	svc := &mockDepartmentService{
		getFn: func(_ context.Context, id uint, _ int, _ bool, _ string) (*dto.DepartmentResponse, error) {
			return nil, &apierr.NotFoundError{Resource: "department", ID: id}
		},
	}

	req := httptest.NewRequest(http.MethodGet, "/departments/999", nil)
	w := httptest.NewRecorder()

	newTestRouter(svc).ServeHTTP(w, req)

	assert.Equal(t, http.StatusNotFound, w.Code)
}

func TestGetDepartment_InvalidID(t *testing.T) {
	svc := &mockDepartmentService{}
	req := httptest.NewRequest(http.MethodGet, "/departments/abc", nil)
	w := httptest.NewRecorder()

	newTestRouter(svc).ServeHTTP(w, req)

	assert.Equal(t, http.StatusBadRequest, w.Code)
}

func TestDeleteDepartment_Cascade(t *testing.T) {
	svc := &mockDepartmentService{
		deleteFn: func(_ context.Context, id uint, mode string, reassignTo *uint) error {
			assert.Equal(t, uint(1), id)
			assert.Equal(t, "cascade", mode)
			assert.Nil(t, reassignTo)
			return nil
		},
	}

	req := httptest.NewRequest(http.MethodDelete, "/departments/1?mode=cascade", nil)
	w := httptest.NewRecorder()

	newTestRouter(svc).ServeHTTP(w, req)

	assert.Equal(t, http.StatusNoContent, w.Code)
}

func TestDeleteDepartment_Reassign(t *testing.T) {
	svc := &mockDepartmentService{
		deleteFn: func(_ context.Context, id uint, mode string, reassignTo *uint) error {
			assert.Equal(t, uint(1), id)
			assert.Equal(t, "reassign", mode)
			require.NotNil(t, reassignTo)
			assert.Equal(t, uint(2), *reassignTo)
			return nil
		},
	}

	req := httptest.NewRequest(http.MethodDelete, "/departments/1?mode=reassign&reassign_to_department_id=2", nil)
	w := httptest.NewRecorder()

	newTestRouter(svc).ServeHTTP(w, req)

	assert.Equal(t, http.StatusNoContent, w.Code)
}


func TestUpdateDepartment_SetParentToNull(t *testing.T) {
	svc := &mockDepartmentService{
		updateFn: func(_ context.Context, id uint, req *dto.UpdateDepartmentRequest) (*dto.DepartmentResponse, error) {
			assert.True(t, req.ParentIDSet)
			assert.True(t, req.ParentIDIsNull)
			assert.Nil(t, req.ParentID)
			return &dto.DepartmentResponse{ID: id, Name: "Engineering"}, nil
		},
	}

	body, _ := json.Marshal(map[string]any{"parent_id": nil})
	req := httptest.NewRequest(http.MethodPatch, "/departments/1", bytes.NewReader(body))
	req.Header.Set("Content-Type", "application/json")
	w := httptest.NewRecorder()

	newTestRouter(svc).ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code)
}

func TestUpdateDepartment_CycleConflict(t *testing.T) {
	svc := &mockDepartmentService{
		updateFn: func(_ context.Context, id uint, req *dto.UpdateDepartmentRequest) (*dto.DepartmentResponse, error) {
			return nil, &apierr.ConflictError{Message: "moving department would create a cycle in the tree"}
		},
	}

	body, _ := json.Marshal(map[string]any{"parent_id": 5})
	req := httptest.NewRequest(http.MethodPatch, "/departments/1", bytes.NewReader(body))
	req.Header.Set("Content-Type", "application/json")
	w := httptest.NewRecorder()

	newTestRouter(svc).ServeHTTP(w, req)

	assert.Equal(t, http.StatusConflict, w.Code)
}

type mockEmployeeService struct{}

func (m *mockEmployeeService) Create(_ context.Context, _ uint, _ *dto.CreateEmployeeRequest) (*dto.EmployeeResponse, error) {
	return nil, nil
}
