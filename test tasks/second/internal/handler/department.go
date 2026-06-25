package handler

import (
	"encoding/json"
	"net/http"
	"strconv"

	"github.com/yourname/org-api/internal/dto"
	"github.com/yourname/org-api/internal/service"
)

type DepartmentHandler struct {
	svc service.DepartmentService
}

func NewDepartmentHandler(svc service.DepartmentService) *DepartmentHandler {
	return &DepartmentHandler{svc: svc}
}

func (h *DepartmentHandler) Create(w http.ResponseWriter, r *http.Request) {
	var req dto.CreateDepartmentRequest
	if err := json.NewDecoder(r.Body).Decode(&req); err != nil {
		writeJSON(w, http.StatusBadRequest, errorResponse{Error: "invalid request body"})
		return
	}

	resp, err := h.svc.Create(r.Context(), &req)
	if err != nil {
		writeError(w, err)
		return
	}
	writeJSON(w, http.StatusCreated, resp)
}

func (h *DepartmentHandler) Get(w http.ResponseWriter, r *http.Request) {
	id, ok := parseUintParam(w, r, "id")
	if !ok {
		return
	}

	depth := queryInt(r, "depth", 1)
	includeEmployees := queryBool(r, "include_employees", true)
	sortBy := r.URL.Query().Get("sort_by")
	if sortBy == "" {
		sortBy = "created_at"
	}

	resp, err := h.svc.Get(r.Context(), id, depth, includeEmployees, sortBy)
	if err != nil {
		writeError(w, err)
		return
	}
	writeJSON(w, http.StatusOK, resp)
}

// PATCH /departments/{id}
func (h *DepartmentHandler) Update(w http.ResponseWriter, r *http.Request) {
	id, ok := parseUintParam(w, r, "id")
	if !ok {
		return
	}

	var raw map[string]json.RawMessage
	if err := json.NewDecoder(r.Body).Decode(&raw); err != nil {
		writeJSON(w, http.StatusBadRequest, errorResponse{Error: "invalid request body"})
		return
	}

	req := &dto.UpdateDepartmentRequest{}

	if nameRaw, ok := raw["name"]; ok {
		var name string
		if err := json.Unmarshal(nameRaw, &name); err != nil {
			writeJSON(w, http.StatusBadRequest, errorResponse{Error: "invalid name"})
			return
		}
		req.Name = &name
	}

	if parentRaw, ok := raw["parent_id"]; ok {
		req.ParentIDSet = true
		if string(parentRaw) == "null" {
			req.ParentIDIsNull = true
			req.ParentID = nil
		} else {
			var pid uint
			if err := json.Unmarshal(parentRaw, &pid); err != nil {
				writeJSON(w, http.StatusBadRequest, errorResponse{Error: "invalid parent_id"})
				return
			}
			req.ParentID = &pid
		}
	}

	resp, err := h.svc.Update(r.Context(), id, req)
	if err != nil {
		writeError(w, err)
		return
	}
	writeJSON(w, http.StatusOK, resp)
}

func (h *DepartmentHandler) Delete(w http.ResponseWriter, r *http.Request) {
	id, ok := parseUintParam(w, r, "id")
	if !ok {
		return
	}

	mode := r.URL.Query().Get("mode")
	var reassignTo *uint
	if raw := r.URL.Query().Get("reassign_to_department_id"); raw != "" {
		v, err := strconv.ParseUint(raw, 10, 64)
		if err != nil {
			writeJSON(w, http.StatusBadRequest, errorResponse{Error: "invalid reassign_to_department_id"})
			return
		}
		u := uint(v)
		reassignTo = &u
	}

	if err := h.svc.Delete(r.Context(), id, mode, reassignTo); err != nil {
		writeError(w, err)
		return
	}
	w.WriteHeader(http.StatusNoContent)
}

func parseUintParam(w http.ResponseWriter, r *http.Request, name string) (uint, bool) {
	raw := r.PathValue(name)
	v, err := strconv.ParseUint(raw, 10, 64)
	if err != nil {
		writeJSON(w, http.StatusBadRequest, errorResponse{Error: "invalid " + name})
		return 0, false
	}
	return uint(v), true
}

func queryInt(r *http.Request, key string, def int) int {
	if raw := r.URL.Query().Get(key); raw != "" {
		if v, err := strconv.Atoi(raw); err == nil {
			return v
		}
	}
	return def
}

func queryBool(r *http.Request, key string, def bool) bool {
	raw := r.URL.Query().Get(key)
	if raw == "" {
		return def
	}
	v, err := strconv.ParseBool(raw)
	if err != nil {
		return def
	}
	return v
}
