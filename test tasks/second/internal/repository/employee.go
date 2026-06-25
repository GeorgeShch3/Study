package repository

import (
	"context"

	"github.com/yourname/org-api/internal/model"
	"gorm.io/gorm"
)

type EmployeeRepository interface {
	Create(ctx context.Context, emp *model.Employee) error
	FindByDepartmentID(ctx context.Context, deptID uint, sortBy string) ([]model.Employee, error)
}

type employeeRepo struct {
	db *gorm.DB
}

func NewEmployeeRepository(db *gorm.DB) EmployeeRepository {
	return &employeeRepo{db: db}
}

func (r *employeeRepo) Create(ctx context.Context, emp *model.Employee) error {
	return r.db.WithContext(ctx).Create(emp).Error
}

func (r *employeeRepo) FindByDepartmentID(ctx context.Context, deptID uint, sortBy string) ([]model.Employee, error) {
	allowed := map[string]bool{"created_at": true, "full_name": true}
	if !allowed[sortBy] {
		sortBy = "created_at"
	}

	var employees []model.Employee
	err := r.db.WithContext(ctx).
		Where("department_id = ?", deptID).
		Order(sortBy).
		Find(&employees).Error
	return employees, err
}
