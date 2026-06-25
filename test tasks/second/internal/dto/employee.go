package dto

import "time"


type CreateEmployeeRequest struct {
	FullName string     `json:"full_name"`
	Position string     `json:"position"`
	HiredAt  *time.Time `json:"hired_at"`
}


type EmployeeResponse struct {
	ID           uint       `json:"id"`
	DepartmentID uint       `json:"department_id"`
	FullName     string     `json:"full_name"`
	Position     string     `json:"position"`
	HiredAt      *time.Time `json:"hired_at"`
	CreatedAt    time.Time  `json:"created_at"`
}
