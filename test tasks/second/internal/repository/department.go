package repository

import (
	"context"
	"database/sql"

	"github.com/yourname/org-api/internal/model"
	"gorm.io/gorm"
)

type TreeRow struct {
	ID        uint
	Name      string
	ParentID  *uint
	CreatedAt sql.NullTime
	Depth     int
}

type DepartmentRepository interface {
	Create(ctx context.Context, dept *model.Department) error
	FindByID(ctx context.Context, id uint) (*model.Department, error)
	Update(ctx context.Context, dept *model.Department) error
	Delete(ctx context.Context, id uint) error

	FindByIDTx(ctx context.Context, tx *gorm.DB, id uint) (*model.Department, error)
	UpdateTx(ctx context.Context, tx *gorm.DB, dept *model.Department) error

	ExistsByNameAndParent(ctx context.Context, name string, parentID *uint, excludeID uint) (bool, error)
	HasCycle(ctx context.Context, deptID uint, newParentID uint) (bool, error)
	GetTree(ctx context.Context, id uint, depth int) ([]TreeRow, error)

	ReassignEmployeesTx(ctx context.Context, tx *gorm.DB, fromDeptID, toDeptID uint) error
	DeleteTx(ctx context.Context, tx *gorm.DB, id uint) error

	DB() *gorm.DB
}

type departmentRepo struct {
	db *gorm.DB
}

func NewDepartmentRepository(db *gorm.DB) DepartmentRepository {
	return &departmentRepo{db: db}
}

func (r *departmentRepo) DB() *gorm.DB { return r.db }

func (r *departmentRepo) Create(ctx context.Context, dept *model.Department) error {
	return r.db.WithContext(ctx).Create(dept).Error
}

func (r *departmentRepo) FindByID(ctx context.Context, id uint) (*model.Department, error) {
	var dept model.Department
	err := r.db.WithContext(ctx).First(&dept, id).Error
	if err != nil {
		return nil, err
	}
	return &dept, nil
}

func (r *departmentRepo) Update(ctx context.Context, dept *model.Department) error {
	return r.db.WithContext(ctx).Save(dept).Error
}

func (r *departmentRepo) Delete(ctx context.Context, id uint) error {
	return r.db.WithContext(ctx).Delete(&model.Department{}, id).Error
}

func (r *departmentRepo) FindByIDTx(ctx context.Context, tx *gorm.DB, id uint) (*model.Department, error) {
	var dept model.Department
	err := tx.WithContext(ctx).Set("gorm:query_option", "FOR UPDATE").First(&dept, id).Error
	if err != nil {
		return nil, err
	}
	return &dept, nil
}

func (r *departmentRepo) UpdateTx(ctx context.Context, tx *gorm.DB, dept *model.Department) error {
	return tx.WithContext(ctx).Save(dept).Error
}

func (r *departmentRepo) ExistsByNameAndParent(ctx context.Context, name string, parentID *uint, excludeID uint) (bool, error) {
	q := r.db.WithContext(ctx).Model(&model.Department{}).
		Where("name = ?", name).
		Where("id != ?", excludeID)

	if parentID == nil {
		q = q.Where("parent_id IS NULL")
	} else {
		q = q.Where("parent_id = ?", *parentID)
	}

	var count int64
	err := q.Count(&count).Error
	return count > 0, err
}

func (r *departmentRepo) HasCycle(ctx context.Context, deptID uint, newParentID uint) (bool, error) {
	query := `
		WITH RECURSIVE ancestors AS (
			SELECT id, parent_id FROM departments WHERE id = $1
			UNION ALL
			SELECT d.id, d.parent_id
			FROM departments d
			JOIN ancestors a ON d.id = a.parent_id
		)
		SELECT COUNT(1) FROM ancestors WHERE id = $2
	`
	var count int64
	err := r.db.WithContext(ctx).Raw(query, newParentID, deptID).Scan(&count).Error
	return count > 0, err
}

func (r *departmentRepo) GetTree(ctx context.Context, id uint, depth int) ([]TreeRow, error) {
	query := `
		WITH RECURSIVE tree AS (
			SELECT id, name, parent_id, created_at, 0 AS depth
			FROM departments
			WHERE id = $1

			UNION ALL

			SELECT d.id, d.name, d.parent_id, d.created_at, t.depth + 1
			FROM departments d
			JOIN tree t ON d.parent_id = t.id
			WHERE t.depth < $2
		)
		SELECT id, name, parent_id, created_at, depth FROM tree ORDER BY depth, id
	`
	var rows []TreeRow
	err := r.db.WithContext(ctx).Raw(query, id, depth).Scan(&rows).Error
	return rows, err
}


func (r *departmentRepo) ReassignEmployeesTx(ctx context.Context, tx *gorm.DB, fromDeptID, toDeptID uint) error {
	return tx.WithContext(ctx).
		Model(&model.Employee{}).
		Where("department_id = ?", fromDeptID).
		Update("department_id", toDeptID).Error
}

func (r *departmentRepo) DeleteTx(ctx context.Context, tx *gorm.DB, id uint) error {
	return tx.WithContext(ctx).Delete(&model.Department{}, id).Error
}
