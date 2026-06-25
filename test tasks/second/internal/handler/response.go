package handler

import (
	"encoding/json"
	"net/http"

	"github.com/yourname/org-api/internal/apierr"
)

type errorResponse struct {
	Error string `json:"error"`
}

func writeJSON(w http.ResponseWriter, status int, data any) {
	w.Header().Set("Content-Type", "application/json")
	w.WriteHeader(status)
	json.NewEncoder(w).Encode(data)
}

func writeError(w http.ResponseWriter, err error) {
	switch e := err.(type) {
	case *apierr.NotFoundError:
		writeJSON(w, http.StatusNotFound, errorResponse{Error: e.Error()})
	case *apierr.ConflictError:
		writeJSON(w, http.StatusConflict, errorResponse{Error: e.Error()})
	case *apierr.ValidationError:
		writeJSON(w, http.StatusBadRequest, errorResponse{Error: e.Error()})
	default:
		writeJSON(w, http.StatusInternalServerError, errorResponse{Error: "internal server error"})
	}
}
