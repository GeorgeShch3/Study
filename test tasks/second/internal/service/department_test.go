package service_test

import (
	"context"
	"database/sql"
	"errors"
	"strings"
	"testing"
	"time"

	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
	"github.com/yourname/org-api/internal/apierr"
	"github.com/yourname/org-api/internal/dto"
	"github.com/yourname/org-api/internal/model"
	"github.com/yourname/org-api/internal/repository"
	"github.com/yourname/org-api/internal/service"
	"gorm.io/gorm"
)

type mockDeptRepo struct {
	createFn                func(context.Context, *model.Department) error
	findByIDFn              func(context.Context, uint) (*model.Department, error)
	existsByNameAndParentFn func(context.Context, string, *uint, uint) (bool, error)
	getTreeFn               func(context.Context, uint, int) ([]repository.TreeRow, error)
	hasCycleFn              func(context.Context, uint, uint) (bool, error)
	dbFn                    func() *gorm.DB
}

func (m *mockDeptRepo) Create(ctx context.Context, dept *model.Department) error {
	if m.createFn != nil {
		return m.createFn(ctx, dept)
	}
	dept.ID = 1
	dept.CreatedAt = time.Now()
	return nil
}
func (m *mockDeptRepo) FindByID(ctx context.Context, id uint) (*model.Department, error) {
	if m.findByIDFn != nil {
		return m.findByIDFn(ctx, id)
	}
	return &model.Department{ID: id, Name: "Test"}, nil
}
func (m *mockDeptRepo) Update(ctx context.Context, dept *model.Department) error { return nil }
func (m *mockDeptRepo) Delete(ctx context.Context, id uint) error                { return nil }
func (m *mockDeptRepo) FindByIDTx(ctx context.Context, tx *gorm.DB, id uint) (*model.Department, error) {
	return &model.Department{ID: id}, nil
}
func (m *mockDeptRepo) UpdateTx(ctx context.Context, tx *gorm.DB, dept *model.Department) error {
	return nil
}
func (m *mockDeptRepo) ExistsByNameAndParent(ctx context.Context, name string, parentID *uint, excludeID uint) (bool, error) {
	if m.existsByNameAndParentFn != nil {
		return m.existsByNameAndParentFn(ctx, name, parentID, excludeID)
	}
	return false, nil
}
func (m *mockDeptRepo) HasCycle(ctx context.Context, deptID uint, newParentID uint) (bool, error) {
	if m.hasCycleFn != nil {
		return m.hasCycleFn(ctx, deptID, newParentID)
	}
	return false, nil
}
func (m *mockDeptRepo) GetTree(ctx context.Context, id uint, depth int) ([]repository.TreeRow, error) {
	if m.getTreeFn != nil {
		return m.getTreeFn(ctx, id, depth)
	}
	return nil, nil
}
func (m *mockDeptRepo) ReassignEmployeesTx(ctx context.Context, tx *gorm.DB, from, to uint) error {
	return nil
}
func (m *mockDeptRepo) DeleteTx(ctx context.Context, tx *gorm.DB, id uint) error { return nil }
func (m *mockDeptRepo) DB() *gorm.DB {
	if m.dbFn != nil {
		return m.dbFn()
	}
	return nil
}

type mockEmpRepo struct {
	findFn func(context.Context, uint, string) ([]model.Employee, error)
}

func (m *mockEmpRepo) Create(ctx context.Context, emp *model.Employee) error { return nil }
func (m *mockEmpRepo) FindByDepartmentID(ctx context.Context, deptID uint, sortBy string) ([]model.Employee, error) {
	if m.findFn != nil {
		return m.findFn(ctx, deptID, sortBy)
	}
	return nil, nil
}

func newDeptService(deptRepo repository.DepartmentRepository, empRepo repository.EmployeeRepository) service.DepartmentService {
	return service.NewDepartmentService(deptRepo, empRepo)
}

func ptr[T any](v T) *T { return &v }

func TestCreateDepartment_EmptyName(t *testing.T) {
	svc := newDeptService(&mockDeptRepo{}, &mockEmpRepo{})
	_, err := svc.Create(context.Background(), &dto.CreateDepartmentRequest{Name: ""})
	var ve *apierr.ValidationError
	require.ErrorAs(t, err, &ve)
	assert.Contains(t, ve.Message, "empty")
}

func TestCreateDepartment_SpacesOnlyName(t *testing.T) {
	svc := newDeptService(&mockDeptRepo{}, &mockEmpRepo{})
	_, err := svc.Create(context.Background(), &dto.CreateDepartmentRequest{Name: "   "})
	var ve *apierr.ValidationError
	require.ErrorAs(t, err, &ve)
}

func TestCreateDepartment_NameTooLong(t *testing.T) {
	svc := newDeptService(&mockDeptRepo{}, &mockEmpRepo{})
	_, err := svc.Create(context.Background(), &dto.CreateDepartmentRequest{
		Name: strings.Repeat("a", 201),
	})
	var ve *apierr.ValidationError
	require.ErrorAs(t, err, &ve)
	assert.Contains(t, ve.Message, "200")
}

func TestCreateDepartment_NameMaxLength(t *testing.T) {
	// ровно 200 символов — должно пройти
	svc := newDeptService(&mockDeptRepo{}, &mockEmpRepo{})
	resp, err := svc.Create(context.Background(), &dto.CreateDepartmentRequest{
		Name: strings.Repeat("a", 200),
	})
	require.NoError(t, err)
	assert.Equal(t, strings.Repeat("a", 200), resp.Name)
}

func TestCreateDepartment_NameTrimmed(t *testing.T) {
	// пробелы по краям должны триммиться
	var createdName string
	repo := &mockDeptRepo{
		createFn: func(_ context.Context, dept *model.Department) error {
			createdName = dept.Name
			dept.ID = 1
			return nil
		},
	}
	svc := newDeptService(repo, &mockEmpRepo{})
	_, err := svc.Create(context.Background(), &dto.CreateDepartmentRequest{Name: "  Engineering  "})
	require.NoError(t, err)
	assert.Equal(t, "Engineering", createdName)
}

func TestCreateDepartment_DuplicateNameSameParent(t *testing.T) {
	repo := &mockDeptRepo{
		existsByNameAndParentFn: func(_ context.Context, name string, parentID *uint, excludeID uint) (bool, error) {
			return true, nil // имя уже занято
		},
	}
	svc := newDeptService(repo, &mockEmpRepo{})
	_, err := svc.Create(context.Background(), &dto.CreateDepartmentRequest{
		Name:     "Backend",
		ParentID: ptr(uint(1)),
	})
	var ce *apierr.ConflictError
	require.ErrorAs(t, err, &ce)
}

func TestCreateDepartment_DuplicateNameRootLevel(t *testing.T) {
	repo := &mockDeptRepo{
		existsByNameAndParentFn: func(_ context.Context, _ string, parentID *uint, _ uint) (bool, error) {
			return parentID == nil, nil
		},
	}
	svc := newDeptService(repo, &mockEmpRepo{})
	_, err := svc.Create(context.Background(), &dto.CreateDepartmentRequest{Name: "Engineering"})
	var ce *apierr.ConflictError
	require.ErrorAs(t, err, &ce)
}

func TestCreateDepartment_DBError(t *testing.T) {
	repo := &mockDeptRepo{
		createFn: func(_ context.Context, _ *model.Department) error {
			return errors.New("connection refused")
		},
	}
	svc := newDeptService(repo, &mockEmpRepo{})
	_, err := svc.Create(context.Background(), &dto.CreateDepartmentRequest{Name: "Engineering"})
	require.Error(t, err)
	assert.Contains(t, err.Error(), "connection refused")
}

func TestGetDepartment_NotFound(t *testing.T) {
	repo := &mockDeptRepo{
		getTreeFn: func(_ context.Context, _ uint, _ int) ([]repository.TreeRow, error) {
			return []repository.TreeRow{}, nil // пустой результат
		},
	}
	svc := newDeptService(repo, &mockEmpRepo{})
	_, err := svc.Get(context.Background(), 999, 1, false, "created_at")
	var nfe *apierr.NotFoundError
	require.ErrorAs(t, err, &nfe)
	assert.Equal(t, uint(999), nfe.ID)
}

func TestGetDepartment_DepthClampedToMax(t *testing.T) {
	var receivedDepth int
	repo := &mockDeptRepo{
		getTreeFn: func(_ context.Context, id uint, depth int) ([]repository.TreeRow, error) {
			receivedDepth = depth
			return []repository.TreeRow{
				{ID: id, Name: "Root", Depth: 0, CreatedAt: sql.NullTime{Time: time.Now(), Valid: true}},
			}, nil
		},
	}
	svc := newDeptService(repo, &mockEmpRepo{})
	_, err := svc.Get(context.Background(), 1, 99, false, "created_at")
	require.NoError(t, err)
	assert.Equal(t, 5, receivedDepth, "depth должен быть обрезан до 5")
}

func TestGetDepartment_DepthClampedToMin(t *testing.T) {
	var receivedDepth int
	repo := &mockDeptRepo{
		getTreeFn: func(_ context.Context, id uint, depth int) ([]repository.TreeRow, error) {
			receivedDepth = depth
			return []repository.TreeRow{
				{ID: id, Name: "Root", Depth: 0, CreatedAt: sql.NullTime{Time: time.Now(), Valid: true}},
			}, nil
		},
	}
	svc := newDeptService(repo, &mockEmpRepo{})
	_, err := svc.Get(context.Background(), 1, 0, false, "created_at")
	require.NoError(t, err)
	assert.Equal(t, 1, receivedDepth, "depth должен быть минимум 1")
}

func TestGetDepartment_TreeAssembledCorrectly(t *testing.T) {
	now := time.Now()
	repo := &mockDeptRepo{
		getTreeFn: func(_ context.Context, id uint, depth int) ([]repository.TreeRow, error) {
			return []repository.TreeRow{
				{ID: 1, Name: "Root", Depth: 0, CreatedAt: sql.NullTime{Time: now, Valid: true}},
				{ID: 2, Name: "Backend", ParentID: ptr(uint(1)), Depth: 1, CreatedAt: sql.NullTime{Time: now, Valid: true}},
				{ID: 3, Name: "Frontend", ParentID: ptr(uint(1)), Depth: 1, CreatedAt: sql.NullTime{Time: now, Valid: true}},
				{ID: 4, Name: "Go Team", ParentID: ptr(uint(2)), Depth: 2, CreatedAt: sql.NullTime{Time: now, Valid: true}},
			}, nil
		},
	}
	svc := newDeptService(repo, &mockEmpRepo{})
	resp, err := svc.Get(context.Background(), 1, 2, false, "created_at")
	require.NoError(t, err)

	assert.Equal(t, uint(1), resp.ID)
	assert.Equal(t, "Root", resp.Name)
	require.Len(t, resp.Children, 2)

	var backend *dto.DepartmentResponse
	for _, c := range resp.Children {
		if c.Name == "Backend" {
			backend = c
		}
	}
	require.NotNil(t, backend)
	require.Len(t, backend.Children, 1)
	assert.Equal(t, "Go Team", backend.Children[0].Name)
}

func TestGetDepartment_NoEmployeesWhenFlagFalse(t *testing.T) {
	now := time.Now()
	empCallCount := 0
	repo := &mockDeptRepo{
		getTreeFn: func(_ context.Context, id uint, _ int) ([]repository.TreeRow, error) {
			return []repository.TreeRow{
				{ID: id, Name: "Root", Depth: 0, CreatedAt: sql.NullTime{Time: now, Valid: true}},
			}, nil
		},
	}
	empRepo := &mockEmpRepo{
		findFn: func(_ context.Context, _ uint, _ string) ([]model.Employee, error) {
			empCallCount++
			return []model.Employee{{ID: 1, FullName: "Иван"}}, nil
		},
	}
	svc := newDeptService(repo, empRepo)
	resp, err := svc.Get(context.Background(), 1, 1, false, "created_at")
	require.NoError(t, err)
	assert.Empty(t, resp.Employees)
	assert.Equal(t, 0, empCallCount, "запрос сотрудников не должен выполняться")
}

func TestGetDepartment_EmployeesSortedByField(t *testing.T) {
	now := time.Now()
	var receivedSortBy string
	repo := &mockDeptRepo{
		getTreeFn: func(_ context.Context, id uint, _ int) ([]repository.TreeRow, error) {
			return []repository.TreeRow{
				{ID: id, Name: "Root", Depth: 0, CreatedAt: sql.NullTime{Time: now, Valid: true}},
			}, nil
		},
	}
	empRepo := &mockEmpRepo{
		findFn: func(_ context.Context, _ uint, sortBy string) ([]model.Employee, error) {
			receivedSortBy = sortBy
			return nil, nil
		},
	}
	svc := newDeptService(repo, empRepo)
	_, err := svc.Get(context.Background(), 1, 1, true, "full_name")
	require.NoError(t, err)
	assert.Equal(t, "full_name", receivedSortBy)
}

func TestDeleteDepartment_InvalidMode(t *testing.T) {
	svc := newDeptService(&mockDeptRepo{}, &mockEmpRepo{})
	err := svc.Delete(context.Background(), 1, "unknown", nil)
	var ve *apierr.ValidationError
	require.ErrorAs(t, err, &ve)
	assert.Contains(t, ve.Message, "mode")
}

func TestDeleteDepartment_ReassignWithoutTarget(t *testing.T) {
	svc := newDeptService(&mockDeptRepo{}, &mockEmpRepo{})
	err := svc.Delete(context.Background(), 1, "reassign", nil)
	var ve *apierr.ValidationError
	require.ErrorAs(t, err, &ve)
	assert.Contains(t, ve.Message, "reassign_to_department_id")
}
