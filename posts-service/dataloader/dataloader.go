package dataloader

import (
	"context"
	"github.com/vikstrous/dataloadgen"
	"posts-service/internal/model"
	"posts-service/internal/repository"
	"time"
)

type Loaders struct {
	CommentsByPostID  *dataloadgen.Loader[repository.CommentPage, *model.CommentConnection]
	RepliesByParentID *dataloadgen.Loader[repository.CommentPage, *model.CommentConnection]
}

func NewLoaders(repo repository.CommentRepository) *Loaders {
	return &Loaders{CommentsByPostID: newPageLoader(repo.GetCommentPages), RepliesByParentID: newPageLoader(repo.GetReplyPages)}
}

func newPageLoader(fetch func(context.Context, []repository.CommentPage) ([]*model.CommentConnection, error)) *dataloadgen.Loader[repository.CommentPage, *model.CommentConnection] {
	return dataloadgen.NewLoader(func(ctx context.Context, keys []repository.CommentPage) ([]*model.CommentConnection, []error) {
		values, err := fetch(ctx, keys)
		errors := make([]error, len(keys))
		if err != nil {
			for i := range errors {
				errors[i] = err
			}
		}
		return values, errors
	}, dataloadgen.WithWait(2*time.Millisecond), dataloadgen.WithBatchCapacity(100))
}
