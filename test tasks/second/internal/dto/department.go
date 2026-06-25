package dto

import "time"


type CreateDepartmentRequest struct {
	Name     string `json:"name"`
	ParentID *uint  `json:"parent_id"`
}


type UpdateDepartmentRequest struct {
	Name           *string
	ParentID       *uint
	ParentIDIsNull bool  
	ParentIDSet    bool 
}


type DepartmentResponse struct {
	ID        uint                  `json:"id"`
	Name      string                `json:"name"`
	ParentID  *uint                 `json:"parent_id"`
	CreatedAt time.Time             `json:"created_at"`
	Employees []*EmployeeResponse   `json:"employees,omitempty"`
	Children  []*DepartmentResponse `json:"children,omitempty"`
}
