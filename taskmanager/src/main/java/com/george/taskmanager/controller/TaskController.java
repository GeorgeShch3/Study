package com.george.taskmanager.controller;

import com.george.taskmanager.dto.Api.*;
import com.george.taskmanager.service.TaskService;

import jakarta.validation.Valid;

import org.springframework.http.HttpStatus;
import org.springframework.web.bind.annotation.*;

import java.security.Principal;

@RestController
@RequestMapping("/api/tasks/{id}")
public class TaskController {
    private final TaskService tasks;

    public TaskController(TaskService tasks) {
        this.tasks = tasks;
    }

    @GetMapping
    public TaskView get(@PathVariable Long id, Principal p) {
        return tasks.get(id, p.getName());
    }

    @PatchMapping
    public TaskView edit(@PathVariable Long id, Principal p, @Valid @RequestBody EditTask r) {
        return tasks.edit(id, p.getName(), r);
    }

    @PutMapping("/assignment")
    public TaskView assign(@PathVariable Long id, Principal p, @Valid @RequestBody AssignTask r) {
        return tasks.assign(id, p.getName(), r);
    }

    @PostMapping("/transitions")
    public TaskView transition(
            @PathVariable Long id, Principal p, @Valid @RequestBody Transition r) {
        return tasks.transition(id, p.getName(), r);
    }

    @PostMapping("/comments")
    @ResponseStatus(HttpStatus.CREATED)
    public CommentView comment(
            @PathVariable Long id, Principal p, @Valid @RequestBody NewComment r) {
        return tasks.comment(id, p.getName(), r);
    }

    @GetMapping("/comments")
    public PageView<CommentView> comments(
            @PathVariable Long id,
            Principal p,
            @RequestParam(defaultValue = "0") int page,
            @RequestParam(defaultValue = "20") int size) {
        return tasks.comments(id, p.getName(), page, size);
    }

    @GetMapping("/history")
    public PageView<EventView> history(
            @PathVariable Long id,
            Principal p,
            @RequestParam(defaultValue = "0") int page,
            @RequestParam(defaultValue = "20") int size) {
        return tasks.history(id, p.getName(), page, size);
    }
}
