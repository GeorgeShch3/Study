package handler

import (
	"log/slog"
	"net/http"
)

func NewRouterNoMiddleware(deptH *DepartmentHandler, empH *EmployeeHandler) http.Handler {
	mux := http.NewServeMux()
	mux.HandleFunc("POST /departments/", deptH.Create)
	mux.HandleFunc("GET /departments/{id}", deptH.Get)
	mux.HandleFunc("PATCH /departments/{id}", deptH.Update)
	mux.HandleFunc("DELETE /departments/{id}", deptH.Delete)
	mux.HandleFunc("POST /departments/{id}/employees/", empH.Create)
	return mux
}

func NewRouter(
	logger *slog.Logger,
	deptH *DepartmentHandler,
	empH *EmployeeHandler,
) http.Handler {
	mux := http.NewServeMux()

	mux.HandleFunc("POST /departments/", deptH.Create)
	mux.HandleFunc("GET /departments/{id}", deptH.Get)
	mux.HandleFunc("PATCH /departments/{id}", deptH.Update)
	mux.HandleFunc("DELETE /departments/{id}", deptH.Delete)

	mux.HandleFunc("POST /departments/{id}/employees/", empH.Create)

	var h http.Handler = mux
	h = LoggingMiddleware(logger)(h)
	h = RequestIDMiddleware(h)
	h = RecoveryMiddleware(logger)(h)

	return h
}
