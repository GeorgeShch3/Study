package apierr

import "fmt"

type NotFoundError struct {
	Resource string
	ID       uint
}

func (e *NotFoundError) Error() string {
	return fmt.Sprintf("%s with id=%d not found", e.Resource, e.ID)
}

type ConflictError struct {
	Message string
}

func (e *ConflictError) Error() string { return e.Message }

type ValidationError struct {
	Message string
}

func (e *ValidationError) Error() string { return e.Message }
