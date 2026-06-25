package service

import (
	"context"
	"errors"
	"strings"

	"github.com/yourname/org-api/internal/apierr"
	"github.com/yourname/org-api/internal/dto"
	"github.com/yourname/org-api/internal/model"
	"github.com/yourname/org-api/internal/repository"
	"gorm.io/gorm"
)

type EmployeeService interface {
	Create(ctx context.Context, deptID uint, req *dto.CreateEmployeeRequest) (*dto.EmployeeResponse, error)
}

type employeeService struct {
	deptRepo repository.DepartmentRepository
	empRepo  repository.EmployeeRepository
}

func NewEmployeeService(deptRepo repository.DepartmentRepository, empRepo repository.EmployeeRepository) EmployeeService {
	return &employeeService{deptRepo: deptRepo, empRepo: empRepo}
}

func (s *employeeService) Create(ctx context.Context, deptID uint, req *dto.CreateEmployeeRequest) (*dto.EmployeeResponse, error) {
	_, err := s.deptRepo.FindByID(ctx, deptID)
	if err != nil {
		if errors.Is(err, gorm.ErrRecordNotFound) {
			return nil, &apierr.NotFoundError{Resource: "department", ID: deptID}
		}
		return nil, err
	}

	fullName := strings.TrimSpace(req.FullName)
	if fullName == "" {
		return nil, &apierr.ValidationError{Message: "full_name must not be empty"}
	}
	if len(fullName) > 200 {
		return nil, &apierr.ValidationError{Message: "full_name must be at most 200 characters"}
	}

	position := strings.TrimSpace(req.Position)
	if position == "" {
		return nil, &apierr.ValidationError{Message: "position must not be empty"}
	}
	if len(position) > 200 {
		return nil, &apierr.ValidationError{Message: "position must be at most 200 characters"}
	}

	emp := &model.Employee{
		DepartmentID: deptID,
		FullName:     fullName,
		Position:     position,
		HiredAt:      req.HiredAt,
	}
	if err := s.empRepo.Create(ctx, emp); err != nil {
		return nil, err
	}

	return empToResponse(emp), nil
}
