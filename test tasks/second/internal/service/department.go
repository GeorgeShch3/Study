package service

import (
	"context"
	"database/sql"
	"errors"
	"strings"

	"github.com/yourname/org-api/internal/apierr"
	"github.com/yourname/org-api/internal/dto"
	"github.com/yourname/org-api/internal/model"
	"github.com/yourname/org-api/internal/repository"
	"gorm.io/gorm"
)

type DepartmentService interface {
	Create(ctx context.Context, req *dto.CreateDepartmentRequest) (*dto.DepartmentResponse, error)
	Get(ctx context.Context, id uint, depth int, includeEmployees bool, sortBy string) (*dto.DepartmentResponse, error)
	Update(ctx context.Context, id uint, req *dto.UpdateDepartmentRequest) (*dto.DepartmentResponse, error)
	Delete(ctx context.Context, id uint, mode string, reassignTo *uint) error
}

type departmentService struct {
	deptRepo repository.DepartmentRepository
	empRepo  repository.EmployeeRepository
}

func NewDepartmentService(deptRepo repository.DepartmentRepository, empRepo repository.EmployeeRepository) DepartmentService {
	return &departmentService{deptRepo: deptRepo, empRepo: empRepo}
}

func (s *departmentService) Create(ctx context.Context, req *dto.CreateDepartmentRequest) (*dto.DepartmentResponse, error) {
	name := strings.TrimSpace(req.Name)
	if name == "" {
		return nil, &apierr.ValidationError{Message: "name must not be empty"}
	}
	if len(name) > 200 {
		return nil, &apierr.ValidationError{Message: "name must be at most 200 characters"}
	}

	exists, err := s.deptRepo.ExistsByNameAndParent(ctx, name, req.ParentID, 0)
	if err != nil {
		return nil, err
	}
	if exists {
		return nil, &apierr.ConflictError{Message: "department with this name already exists in the given parent"}
	}

	dept := &model.Department{
		Name:     name,
		ParentID: req.ParentID,
	}
	if err := s.deptRepo.Create(ctx, dept); err != nil {
		return nil, err
	}

	return deptToResponse(dept, nil, nil), nil
}

func (s *departmentService) Get(ctx context.Context, id uint, depth int, includeEmployees bool, sortBy string) (*dto.DepartmentResponse, error) {
	// Ограничиваем depth
	if depth < 1 {
		depth = 1
	}
	if depth > 5 {
		depth = 5
	}

	rows, err := s.deptRepo.GetTree(ctx, id, depth)
	if err != nil {
		return nil, err
	}
	if len(rows) == 0 {
		return nil, &apierr.NotFoundError{Resource: "department", ID: id}
	}

	var deptIDs []uint
	for _, row := range rows {
		deptIDs = append(deptIDs, row.ID)
	}

	empsByDept := make(map[uint][]*dto.EmployeeResponse)
	if includeEmployees {
		for _, deptID := range deptIDs {
			emps, err := s.empRepo.FindByDepartmentID(ctx, deptID, sortBy)
			if err != nil {
				return nil, err
			}
			for _, e := range emps {
				emp := e
				empsByDept[deptID] = append(empsByDept[deptID], empToResponse(&emp))
			}
		}
	}

	return buildTree(rows, empsByDept), nil
}

func (s *departmentService) Update(ctx context.Context, id uint, req *dto.UpdateDepartmentRequest) (*dto.DepartmentResponse, error) {
	var result *dto.DepartmentResponse

	err := s.deptRepo.DB().WithContext(ctx).Transaction(func(tx *gorm.DB) error {
		if err := tx.Exec("SET TRANSACTION ISOLATION LEVEL SERIALIZABLE").Error; err != nil {
			return err
		}

		dept, err := s.deptRepo.FindByIDTx(ctx, tx, id)
		if err != nil {
			if errors.Is(err, gorm.ErrRecordNotFound) {
				return &apierr.NotFoundError{Resource: "department", ID: id}
			}
			return err
		}

		if req.Name != nil {
			name := strings.TrimSpace(*req.Name)
			if name == "" {
				return &apierr.ValidationError{Message: "name must not be empty"}
			}
			if len(name) > 200 {
				return &apierr.ValidationError{Message: "name must be at most 200 characters"}
			}
			dept.Name = name
		}

		if req.ParentIDSet {
			newParentID := req.ParentID 

			if newParentID != nil && *newParentID == id {
				return &apierr.ConflictError{Message: "department cannot be its own parent"}
			}

			if newParentID != nil {
				hasCycle, err := s.deptRepo.HasCycle(ctx, id, *newParentID)
				if err != nil {
					return err
				}
				if hasCycle {
					return &apierr.ConflictError{Message: "moving department would create a cycle in the tree"}
				}
			}

			dept.ParentID = newParentID
		}

		exists, err := s.deptRepo.ExistsByNameAndParent(ctx, dept.Name, dept.ParentID, id)
		if err != nil {
			return err
		}
		if exists {
			return &apierr.ConflictError{Message: "department with this name already exists in the given parent"}
		}

		if err := s.deptRepo.UpdateTx(ctx, tx, dept); err != nil {
			if strings.Contains(err.Error(), "23503") || strings.Contains(err.Error(), "foreign key") {
				return &apierr.ConflictError{Message: "parent department not found"}
			}
			return err
		}

		result = deptToResponse(dept, nil, nil)
		return nil
	}, &sql.TxOptions{Isolation: sql.LevelSerializable})

	if err != nil {
		return nil, err
	}
	return result, nil
}

func (s *departmentService) Delete(ctx context.Context, id uint, mode string, reassignTo *uint) error {
	if mode != "cascade" && mode != "reassign" {
		return &apierr.ValidationError{Message: "mode must be 'cascade' or 'reassign'"}
	}
	if mode == "reassign" && reassignTo == nil {
		return &apierr.ValidationError{Message: "reassign_to_department_id is required when mode=reassign"}
	}

	return s.deptRepo.DB().WithContext(ctx).Transaction(func(tx *gorm.DB) error {
		if err := tx.Exec("SET TRANSACTION ISOLATION LEVEL SERIALIZABLE").Error; err != nil {
			return err
		}

		dept, err := s.deptRepo.FindByIDTx(ctx, tx, id)
		if err != nil {
			if errors.Is(err, gorm.ErrRecordNotFound) {
				return &apierr.NotFoundError{Resource: "department", ID: id}
			}
			return err
		}
		_ = dept

		if mode == "reassign" {
			_, err := s.deptRepo.FindByIDTx(ctx, tx, *reassignTo)
			if err != nil {
				if errors.Is(err, gorm.ErrRecordNotFound) {
					return &apierr.NotFoundError{Resource: "reassign target department", ID: *reassignTo}
				}
				return err
			}

			if err := s.deptRepo.ReassignEmployeesTx(ctx, tx, id, *reassignTo); err != nil {
				return err
			}
		}

		return s.deptRepo.DeleteTx(ctx, tx, id)
	}, &sql.TxOptions{Isolation: sql.LevelSerializable})
}

func buildTree(rows []repository.TreeRow, empsByDept map[uint][]*dto.EmployeeResponse) *dto.DepartmentResponse {
	nodes := make(map[uint]*dto.DepartmentResponse, len(rows))

	for _, row := range rows {
		r := row
		nodes[r.ID] = &dto.DepartmentResponse{
			ID:        r.ID,
			Name:      r.Name,
			ParentID:  r.ParentID,
			CreatedAt: r.CreatedAt.Time,
			Employees: empsByDept[r.ID],
		}
	}

	for _, row := range rows {
		if row.Depth == 0 {
			continue
		}
		if row.ParentID != nil {
			parent := nodes[*row.ParentID]
			parent.Children = append(parent.Children, nodes[row.ID])
		}
	}

	return nodes[rows[0].ID]
}

func deptToResponse(dept *model.Department, employees []*dto.EmployeeResponse, children []*dto.DepartmentResponse) *dto.DepartmentResponse {
	return &dto.DepartmentResponse{
		ID:        dept.ID,
		Name:      dept.Name,
		ParentID:  dept.ParentID,
		CreatedAt: dept.CreatedAt,
		Employees: employees,
		Children:  children,
	}
}

func empToResponse(emp *model.Employee) *dto.EmployeeResponse {
	return &dto.EmployeeResponse{
		ID:           emp.ID,
		DepartmentID: emp.DepartmentID,
		FullName:     emp.FullName,
		Position:     emp.Position,
		HiredAt:      emp.HiredAt,
		CreatedAt:    emp.CreatedAt,
	}
}
