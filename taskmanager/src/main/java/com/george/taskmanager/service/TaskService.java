package com.george.taskmanager.service;

import static org.springframework.http.HttpStatus.*;

import com.george.taskmanager.dto.Api.*;
import com.george.taskmanager.entity.*;
import com.george.taskmanager.exception.ApiException;
import com.george.taskmanager.repository.*;

import org.springframework.data.domain.*;
import org.springframework.stereotype.Service;
import org.springframework.transaction.annotation.Transactional;

import java.util.Objects;

@Service
@Transactional(readOnly = true)
public class TaskService {
    private final TaskRepository tasks;
    private final CommentRepository comments;
    private final EventRepository events;
    private final ProjectService projects;
    private final AccountService accounts;

    public TaskService(
            TaskRepository tasks,
            CommentRepository comments,
            EventRepository events,
            ProjectService projects,
            AccountService accounts) {
        this.tasks = tasks;
        this.comments = comments;
        this.events = events;
        this.projects = projects;
        this.accounts = accounts;
    }

    private Task accessible(Long id, AppUser actor) {
        var t = tasks.findById(id).orElseThrow(() -> new ApiException(NOT_FOUND, "Task not found"));
        projects.accessible(t.getProject().getId(), actor);
        return t;
    }

    private PageRequest page(int page, int size) {
        if (page < 0 || size < 1 || size > 100)
            throw new ApiException(
                    BAD_REQUEST, "page must be >= 0; size must be between 1 and 100");
        return PageRequest.of(page, size, Sort.by(Sort.Direction.DESC, "id"));
    }

    private void version(Task t, Long expected) {
        if (!Objects.equals(t.getVersion(), expected))
            throw new ApiException(
                    CONFLICT, "Task changed. Reload it and retry with the latest version.");
    }

    private void event(Task t, AppUser actor, String type, String details) {
        events.save(new TaskEvent(t, actor, type, details));
    }

    private void distinct(AppUser a, AppUser r) {
        if (a != null && r != null && a.getId().equals(r.getId()))
            throw new ApiException(BAD_REQUEST, "Assignee and reviewer must be different users");
    }

    private void editable(Task t) {
        if (t.getStatus() == TaskStatus.REVIEW || t.getStatus() == TaskStatus.DONE)
            throw new ApiException(
                    CONFLICT,
                    "Task must be TODO or IN_PROGRESS to change its content or assignment");
    }

    @Transactional
    public TaskView create(Long projectId, String username, NewTask r) {
        var actor = accounts.current(username);
        var p = projects.accessible(projectId, actor);
        if (r.assigneeId() != null || r.reviewerId() != null) projects.owner(p, actor);
        var assignee = projects.member(p, r.assigneeId());
        var reviewer = projects.member(p, r.reviewerId());
        distinct(assignee, reviewer);
        var t =
                tasks.saveAndFlush(
                        new Task(p, actor, r.title().trim(), r.description(), assignee, reviewer));
        event(t, actor, "CREATED", "Task created");
        return TaskView.of(t);
    }

    public PageView<TaskView> list(
            Long projectId, String username, TaskStatus status, int page, int size) {
        projects.accessible(projectId, accounts.current(username));
        var pageable = page(page, size);
        var result =
                status == null
                        ? tasks.findByProjectId(projectId, pageable)
                        : tasks.findByProjectIdAndStatus(projectId, status, pageable);
        return PageView.of(result.map(TaskView::of));
    }

    public TaskView get(Long id, String username) {
        return TaskView.of(accessible(id, accounts.current(username)));
    }

    @Transactional
    public TaskView edit(Long id, String username, EditTask r) {
        var actor = accounts.current(username);
        var t = accessible(id, actor);
        version(t, r.version());
        editable(t);
        if (!t.getAuthor().getId().equals(actor.getId())
                && !t.getProject().getOwner().getId().equals(actor.getId()))
            throw new ApiException(FORBIDDEN, "Only author or owner can edit task content");
        boolean changed = false;
        if (r.title() != null && !r.title().trim().equals(t.getTitle())) {
            event(t, actor, "TITLE_CHANGED", t.getTitle() + " -> " + r.title().trim());
            t.setTitle(r.title().trim());
            changed = true;
        }
        if (r.description() != null && !Objects.equals(r.description(), t.getDescription())) {
            event(
                    t,
                    actor,
                    "DESCRIPTION_CHANGED",
                    "Old: "
                            + Objects.toString(t.getDescription(), "")
                            + "\nNew: "
                            + r.description());
            t.setDescription(r.description());
            changed = true;
        }
        if (!changed) throw new ApiException(BAD_REQUEST, "Provide changed title or description");
        tasks.flush();
        return TaskView.of(t);
    }

    @Transactional
    public TaskView assign(Long id, String username, AssignTask r) {
        var actor = accounts.current(username);
        var t = accessible(id, actor);
        projects.owner(t.getProject(), actor);
        version(t, r.version());
        editable(t);
        var a = projects.member(t.getProject(), r.assigneeId());
        var reviewer = projects.member(t.getProject(), r.reviewerId());
        distinct(a, reviewer);
        if (t.getStatus() == TaskStatus.IN_PROGRESS && a == null)
            throw new ApiException(CONFLICT, "An in-progress task must have an assignee");
        if (Objects.equals(id(t.getAssignee()), id(a))
                && Objects.equals(id(t.getReviewer()), id(reviewer)))
            throw new ApiException(BAD_REQUEST, "Assignment is unchanged");
        event(
                t,
                actor,
                "ASSIGNMENT_CHANGED",
                "Assignee: "
                        + id(t.getAssignee())
                        + " -> "
                        + id(a)
                        + "; reviewer: "
                        + id(t.getReviewer())
                        + " -> "
                        + id(reviewer));
        t.setAssignee(a);
        t.setReviewer(reviewer);
        tasks.flush();
        return TaskView.of(t);
    }

    private Long id(AppUser user) {
        return user == null ? null : user.getId();
    }

    private void requireActor(AppUser expected, AppUser actor, String message) {
        if (expected == null || !expected.getId().equals(actor.getId()))
            throw new ApiException(FORBIDDEN, message);
    }

    @Transactional
    public TaskView transition(Long id, String username, Transition r) {
        var actor = accounts.current(username);
        var t = accessible(id, actor);
        version(t, r.version());
        var from = t.getStatus();
        var to = r.status();
        boolean allowed =
                (from == TaskStatus.TODO && to == TaskStatus.IN_PROGRESS)
                        || (from == TaskStatus.IN_PROGRESS && to == TaskStatus.REVIEW)
                        || (from == TaskStatus.REVIEW
                                && (to == TaskStatus.DONE || to == TaskStatus.IN_PROGRESS));
        if (!allowed) throw new ApiException(CONFLICT, "Invalid transition: " + from + " -> " + to);
        if (from == TaskStatus.REVIEW)
            requireActor(
                    t.getReviewer(),
                    actor,
                    "Only the assigned reviewer can accept or return a task");
        else requireActor(t.getAssignee(), actor, "Only the assignee can start or submit a task");
        if (to == TaskStatus.REVIEW && t.getReviewer() == null)
            throw new ApiException(CONFLICT, "Assign a reviewer before submitting");
        if (from == TaskStatus.REVIEW
                && to == TaskStatus.IN_PROGRESS
                && (r.comment() == null || r.comment().isBlank()))
            throw new ApiException(BAD_REQUEST, "Returning a task requires a comment");
        if (r.comment() != null && !r.comment().isBlank())
            comments.save(new TaskComment(t, actor, r.comment().trim()));
        t.setStatus(to);
        event(t, actor, "STATUS_CHANGED", from + " -> " + to);
        tasks.flush();
        return TaskView.of(t);
    }

    @Transactional
    public CommentView comment(Long id, String username, NewComment r) {
        var actor = accounts.current(username);
        var t = accessible(id, actor);
        return CommentView.of(comments.save(new TaskComment(t, actor, r.body().trim())));
    }

    public PageView<CommentView> comments(Long id, String username, int page, int size) {
        accessible(id, accounts.current(username));
        return PageView.of(comments.findByTaskId(id, page(page, size)).map(CommentView::of));
    }

    public PageView<EventView> history(Long id, String username, int page, int size) {
        accessible(id, accounts.current(username));
        return PageView.of(events.findByTaskId(id, page(page, size)).map(EventView::of));
    }
}
