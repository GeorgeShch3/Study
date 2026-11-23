package com.george.taskmanager.dto;

import com.george.taskmanager.entity.*;

import jakarta.validation.constraints.*;

import org.springframework.data.domain.Page;

import java.time.Instant;
import java.util.List;

public final class Api {
    private Api() {}

    public record Register(
            @NotBlank @Pattern(regexp = "[a-z0-9_]{3,40}") String username,
            @NotBlank @Size(min = 10, max = 72) String password) {
        @Override
        public String toString() {
            return "Register[username=" + username + ", password=[REDACTED]]";
        }
    }

    public record UserView(Long id, String username) {
        public static UserView of(AppUser u) {
            return new UserView(u.getId(), u.getUsername());
        }
    }

    public record NewProject(@NotBlank @Size(max = 120) String name) {}

    public record ProjectView(Long id, String name, Long ownerId) {
        public static ProjectView of(Project p) {
            return new ProjectView(p.getId(), p.getName(), p.getOwner().getId());
        }
    }

    public record AddMember(@NotBlank String username) {}

    public record MemberView(Long userId, String username, String role) {}

    public record NewTask(
            @NotBlank @Size(max = 255) String title,
            @Size(max = 2000) String description,
            @Positive Long assigneeId,
            @Positive Long reviewerId) {}

    public record EditTask(
            @NotNull @PositiveOrZero Long version,
            @Pattern(regexp = "(?s).*\\S.*") @Size(max = 255) String title,
            @Size(max = 2000) String description) {}

    public record AssignTask(
            @NotNull @PositiveOrZero Long version,
            @Positive Long assigneeId,
            @Positive Long reviewerId) {}

    public record Transition(
            @NotNull @PositiveOrZero Long version,
            @NotNull TaskStatus status,
            @Size(max = 2000) String comment) {}

    public record NewComment(@NotBlank @Size(max = 2000) String body) {}

    public record TaskView(
            Long id,
            Long projectId,
            String title,
            String description,
            TaskStatus status,
            Long authorId,
            Long assigneeId,
            Long reviewerId,
            Long version,
            Instant createdAt) {
        public static TaskView of(Task t) {
            return new TaskView(
                    t.getId(),
                    t.getProject().getId(),
                    t.getTitle(),
                    t.getDescription(),
                    t.getStatus(),
                    t.getAuthor().getId(),
                    t.getAssignee() == null ? null : t.getAssignee().getId(),
                    t.getReviewer() == null ? null : t.getReviewer().getId(),
                    t.getVersion(),
                    t.getCreatedAt());
        }
    }

    public record CommentView(Long id, Long authorId, String body, Instant createdAt) {
        public static CommentView of(TaskComment c) {
            return new CommentView(c.getId(), c.getAuthor().getId(), c.getBody(), c.getCreatedAt());
        }
    }

    public record EventView(Long id, Long actorId, String type, String details, Instant createdAt) {
        public static EventView of(TaskEvent e) {
            return new EventView(
                    e.getId(), e.getActor().getId(), e.getType(), e.getDetails(), e.getCreatedAt());
        }
    }

    public record PageView<T>(
            List<T> items, int page, int size, long totalElements, int totalPages) {
        public static <T> PageView<T> of(Page<T> p) {
            return new PageView<>(
                    p.getContent(),
                    p.getNumber(),
                    p.getSize(),
                    p.getTotalElements(),
                    p.getTotalPages());
        }
    }
}
