FROM golang:1.21-alpine AS builder
WORKDIR /build
COPY go.mod go.sum ./
RUN go mod download
COPY . .
RUN CGO_ENABLED=0 GOOS=linux go build -o /out/posts-service ./server

FROM alpine:3.19
RUN apk --no-cache add ca-certificates
WORKDIR /app
COPY --from=builder /out/posts-service ./server
COPY --from=builder /build/migrations ./migrations
EXPOSE 8080
CMD ["./server"]
