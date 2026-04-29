package dataloader

import (
	"context"
	"posts-service/internal/model"
	"posts-service/internal/repository"
	"time"

	"github.com/vikstrous/dataloadgen"
)

const (
	defaultWaitTime = 2 * time.Millisecond
)

type Loaders struct {
	CommentsByPostID  *dataloadgen.Loader[string, []*model.Comment]
	RepliesByParentID *dataloadgen.Loader[string, []*model.Comment]
}

func NewLoaders(commentRepo repository.CommentRepository) *Loaders {
	return &Loaders{
		CommentsByPostID:  newCommentsByPostIDLoader(commentRepo),
		RepliesByParentID: newRepliesByParentIDLoader(commentRepo),
	}
}

func newCommentsByPostIDLoader(repo repository.CommentRepository) *dataloadgen.Loader[string, []*model.Comment] {
	return dataloadgen.NewLoader(
		makeBatchLoader(func(ctx context.Context, ids []string) (map[string][]*model.Comment, error) {
			return repo.GetCommentsByPostIDs(ctx, ids)
		}),
		dataloadgen.WithWait(defaultWaitTime),
	)
}

func newRepliesByParentIDLoader(repo repository.CommentRepository) *dataloadgen.Loader[string, []*model.Comment] {
	return dataloadgen.NewLoader(
		makeBatchLoader(func(ctx context.Context, ids []string) (map[string][]*model.Comment, error) {
			return repo.GetRepliesByParentIDs(ctx, ids)
		}),
		dataloadgen.WithWait(defaultWaitTime),
	)
}

type batchLoaderFunc func(ctx context.Context, ids []string) (map[string][]*model.Comment, error)

func makeBatchLoader(loader batchLoaderFunc) func(ctx context.Context, ids []string) ([][]*model.Comment, []error) {
	return func(ctx context.Context, ids []string) ([][]*model.Comment, []error) {
		dataMap, err := loader(ctx, ids)
		if err != nil {
			return nil, makeGlobalError(ids, err)
		}
		
		result := make([][]*model.Comment, len(ids))
		errors := make([]error, len(ids))
		
		for i, id := range ids {
			if comments, ok := dataMap[id]; ok {
				result[i] = comments
				errors[i] = nil
			} else {
				result[i] = []*model.Comment{}
				errors[i] = nil
			}
		}
		
		return result, errors
	}
}

func makeGlobalError(ids []string, err error) []error {
	errors := make([]error, len(ids))
	for i := range errors {
		errors[i] = err
	}
	return errors
}