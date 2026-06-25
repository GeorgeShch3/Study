package handler

import (
	"encoding/json"
	"net/http"

	"github.com/yourname/org-api/internal/dto"
	"github.com/yourname/org-api/internal/service"
)

type EmployeeHandler struct {
	svc service.EmployeeService
}

func NewEmployeeHandler(svc service.EmployeeService) *EmployeeHandler {
	return &EmployeeHandler{svc: svc}
}

func (h *EmployeeHandler) Create(w http.ResponseWriter, r *http.Request) {
	deptID, ok := parseUintParam(w, r, "id")
	if !ok {
		return
	}

	var req dto.CreateEmployeeRequest
	if err := json.NewDecoder(r.Body).Decode(&req); err != nil {
		writeJSON(w, http.StatusBadRequest, errorResponse{Error: "invalid request body"})
		return
	}

	resp, err := h.svc.Create(r.Context(), deptID, &req)
	if err != nil {
		writeError(w, err)
		return
	}
	writeJSON(w, http.StatusCreated, resp)
}
