package com.george.taskmanager.controller;

import com.george.taskmanager.dto.Api.*;
import com.george.taskmanager.entity.TaskStatus;
import com.george.taskmanager.service.*;

import jakarta.validation.Valid;

import org.springframework.http.HttpStatus;
import org.springframework.web.bind.annotation.*;

import java.security.Principal;
import java.util.List;

@RestController
@RequestMapping("/api/projects")
public class ProjectController {
    private final ProjectService projects;
    private final TaskService tasks;

    public ProjectController(ProjectService projects, TaskService tasks) {
        this.projects = projects;
        this.tasks = tasks;
    }

    @PostMapping
    @ResponseStatus(HttpStatus.CREATED)
    public ProjectView create(Principal p, @Valid @RequestBody NewProject r) {
        return projects.create(p.getName(), r);
    }

    @GetMapping
    public List<ProjectView> list(Principal p) {
        return projects.list(p.getName());
    }

    @PostMapping("/{id}/members")
    @ResponseStatus(HttpStatus.CREATED)
    public MemberView add(@PathVariable Long id, Principal p, @Valid @RequestBody AddMember r) {
        return projects.add(id, p.getName(), r);
    }

    @GetMapping("/{id}/members")
    public List<MemberView> members(@PathVariable Long id, Principal p) {
        return projects.members(id, p.getName());
    }

    @PostMapping("/{id}/tasks")
    @ResponseStatus(HttpStatus.CREATED)
    public TaskView createTask(@PathVariable Long id, Principal p, @Valid @RequestBody NewTask r) {
        return tasks.create(id, p.getName(), r);
    }

    @GetMapping("/{id}/tasks")
    public PageView<TaskView> tasks(
            @PathVariable Long id,
            Principal p,
            @RequestParam(required = false) TaskStatus status,
            @RequestParam(defaultValue = "0") int page,
            @RequestParam(defaultValue = "20") int size) {
        return tasks.list(id, p.getName(), status, page, size);
    }
}
