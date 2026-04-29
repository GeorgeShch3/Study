package dataloader

import (
	"context"
)

type contextKey string

const loadersKey = contextKey("dataloaders")

func WithLoaders(ctx context.Context, loaders *Loaders) context.Context {
	return context.WithValue(ctx, loadersKey, loaders)
}

func GetLoaders(ctx context.Context) *Loaders {
	if loaders, ok := ctx.Value(loadersKey).(*Loaders); ok {
		return loaders
	}
	return nil
}