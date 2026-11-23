package repository

import (
	"fmt"
	"github.com/google/uuid"
	"posts-service/internal/model"
)

const MaxPageSize = 100

type CommentPage struct {
	ParentID string
	First    int
	After    string
}

func NewCommentPage(parentID string, first int, after *string) CommentPage {
	if first <= 0 {
		first = 10
	}
	page := CommentPage{ParentID: parentID, First: first}
	if after != nil {
		page.After = *after
	}
	return page
}

func (p CommentPage) validate() error {
	if p.First < 1 || p.First > MaxPageSize {
		return fmt.Errorf("first must be between 1 and %d", MaxPageSize)
	}
	if _, err := uuid.Parse(p.ParentID); err != nil {
		return fmt.Errorf("invalid parent ID")
	}
	if p.After != "" {
		if _, err := uuid.Parse(p.After); err != nil {
			return fmt.Errorf("invalid after cursor")
		}
	}
	return nil
}

func commentConnection(comments []*model.Comment, first, total int) *model.CommentConnection {
	hasNext := len(comments) > first
	if hasNext {
		comments = comments[:first]
	}
	edges := make([]*model.CommentEdge, len(comments))
	for i, c := range comments {
		edges[i] = &model.CommentEdge{Node: c, Position: c.ID}
	}
	info := &model.PageInfo{HasNextPage: hasNext}
	if len(edges) > 0 {
		info.StartPosition = &edges[0].Position
		info.EndPosition = &edges[len(edges)-1].Position
	}
	return &model.CommentConnection{Edges: edges, PageInfo: info, TotalCount: total}
}
