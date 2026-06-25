package service_test

import (
	"context"
	"strings"
	"testing"

	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
	"github.com/yourname/org-api/internal/apierr"
	"github.com/yourname/org-api/internal/dto"
	"github.com/yourname/org-api/internal/model"
	"github.com/yourname/org-api/internal/service"
	"gorm.io/gorm"
)

type savingEmpRepo struct {
	mockEmpRepo
	saved *model.Employee
}

func (r *savingEmpRepo) Create(_ context.Context, emp *model.Employee) error {
	emp.ID = 42
	r.saved = emp
	return nil
}

func TestCreateEmployee_DepartmentNotFound(t *testing.T) {
	repo := &mockDeptRepo{
		findByIDFn: func(_ context.Context, id uint) (*model.Department, error) {
			return nil, gorm.ErrRecordNotFound
		},
	}
	svc := service.NewEmployeeService(repo, &mockEmpRepo{})
	_, err := svc.Create(context.Background(), 999, &dto.CreateEmployeeRequest{
		FullName: "Иван Иванов",
		Position: "Engineer",
	})
	var nfe *apierr.NotFoundError
	require.ErrorAs(t, err, &nfe)
	assert.Equal(t, uint(999), nfe.ID)
}

func TestCreateEmployee_EmptyFullName(t *testing.T) {
	svc := service.NewEmployeeService(&mockDeptRepo{}, &mockEmpRepo{})
	_, err := svc.Create(context.Background(), 1, &dto.CreateEmployeeRequest{
		FullName: "",
		Position: "Engineer",
	})
	var ve *apierr.ValidationError
	require.ErrorAs(t, err, &ve)
	assert.Contains(t, ve.Message, "full_name")
}

func TestCreateEmployee_SpacesOnlyFullName(t *testing.T) {
	svc := service.NewEmployeeService(&mockDeptRepo{}, &mockEmpRepo{})
	_, err := svc.Create(context.Background(), 1, &dto.CreateEmployeeRequest{
		FullName: "   ",
		Position: "Engineer",
	})
	var ve *apierr.ValidationError
	require.ErrorAs(t, err, &ve)
}

func TestCreateEmployee_FullNameTooLong(t *testing.T) {
	svc := service.NewEmployeeService(&mockDeptRepo{}, &mockEmpRepo{})
	_, err := svc.Create(context.Background(), 1, &dto.CreateEmployeeRequest{
		FullName: strings.Repeat("a", 201),
		Position: "Engineer",
	})
	var ve *apierr.ValidationError
	require.ErrorAs(t, err, &ve)
}

func TestCreateEmployee_EmptyPosition(t *testing.T) {
	svc := service.NewEmployeeService(&mockDeptRepo{}, &mockEmpRepo{})
	_, err := svc.Create(context.Background(), 1, &dto.CreateEmployeeRequest{
		FullName: "Иван Иванов",
		Position: "",
	})
	var ve *apierr.ValidationError
	require.ErrorAs(t, err, &ve)
	assert.Contains(t, ve.Message, "position")
}

func TestCreateEmployee_PositionTooLong(t *testing.T) {
	svc := service.NewEmployeeService(&mockDeptRepo{}, &mockEmpRepo{})
	_, err := svc.Create(context.Background(), 1, &dto.CreateEmployeeRequest{
		FullName: "Иван Иванов",
		Position: strings.Repeat("a", 201),
	})
	var ve *apierr.ValidationError
	require.ErrorAs(t, err, &ve)
}

func TestCreateEmployee_NameAndPositionTrimmed(t *testing.T) {
	empRepo := &savingEmpRepo{}
	svc := service.NewEmployeeService(&mockDeptRepo{}, empRepo)

	resp, err := svc.Create(context.Background(), 1, &dto.CreateEmployeeRequest{
		FullName: "  Иван Иванов  ",
		Position: "  Senior Engineer  ",
	})
	require.NoError(t, err)
	assert.Equal(t, "Иван Иванов", resp.FullName)
	assert.Equal(t, "Senior Engineer", resp.Position)
	require.NotNil(t, empRepo.saved)
	assert.Equal(t, "Иван Иванов", empRepo.saved.FullName)
}

func TestCreateEmployee_Success(t *testing.T) {
	empRepo := &savingEmpRepo{}
	svc := service.NewEmployeeService(&mockDeptRepo{}, empRepo)

	resp, err := svc.Create(context.Background(), 5, &dto.CreateEmployeeRequest{
		FullName: "Пётр Петров",
		Position: "Manager",
	})
	require.NoError(t, err)
	assert.Equal(t, uint(42), resp.ID)
	assert.Equal(t, uint(5), resp.DepartmentID)
	assert.Equal(t, "Пётр Петров", resp.FullName)
	assert.Equal(t, "Manager", resp.Position)
}
