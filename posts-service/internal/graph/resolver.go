package graph

import (
	"context"
	"posts-service/dataloader"
	"posts-service/internal/repository"
)

type Resolver struct {
	PostRepo    repository.PostRepository
	CommentRepo repository.CommentRepository
}

func NewResolver(postRepo repository.PostRepository, commentRepo repository.CommentRepository) *Resolver {
	return &Resolver{
		PostRepo:    postRepo,
		CommentRepo: commentRepo,
	}
}

func (r *Resolver) DataLoaderMiddleware(ctx context.Context) context.Context {
	return dataloader.WithLoaders(ctx, dataloader.NewLoaders(r.CommentRepo))
}
