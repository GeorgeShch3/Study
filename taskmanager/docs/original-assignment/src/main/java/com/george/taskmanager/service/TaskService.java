package com.george.taskmanager.service;

import com.george.taskmanager.dto.TaskCreateRequest;
import com.george.taskmanager.dto.TaskResponse;
import com.george.taskmanager.dto.TaskUpdateRequest;
import com.george.taskmanager.entity.Task;
import com.george.taskmanager.entity.TaskStatus;
import com.george.taskmanager.exception.TaskNotFoundException;
import com.george.taskmanager.repository.TaskRepository;
import org.springframework.stereotype.Service;
import org.springframework.transaction.annotation.Transactional;

import java.util.List;

@Service
@Transactional(readOnly = true)
public class TaskService {

    private final TaskRepository repository;

    public TaskService(TaskRepository repository) {
        this.repository = repository;
    }

    @Transactional
    public TaskResponse create(TaskCreateRequest request) {
        Task task = new Task(request.title(), request.description());
        return TaskResponse.from(repository.save(task));
    }

    public List<TaskResponse> findAll(TaskStatus status) {
        List<Task> tasks = (status == null)
                ? repository.findAllByOrderByCreatedAtDesc()
                : repository.findByStatusOrderByCreatedAtDesc(status);
        return tasks.stream()
                .map(TaskResponse::from)
                .toList();
    }

    public TaskResponse findById(Long id) {
        return TaskResponse.from(getTask(id));
    }

    @Transactional
    public TaskResponse update(Long id, TaskUpdateRequest request) {
        Task task = getTask(id);

        if (request.title() != null) {
            task.setTitle(request.title());
        }
        if (request.description() != null) {
            task.setDescription(request.description());
        }
        if (request.status() != null) {
            task.setStatus(request.status());
        }

        return TaskResponse.from(task);
    }

    @Transactional
    public void delete(Long id) {
        if (!repository.existsById(id)) {
            throw new TaskNotFoundException(id);
        }
        repository.deleteById(id);
    }

    private Task getTask(Long id) {
        return repository.findById(id)
                .orElseThrow(() -> new TaskNotFoundException(id));
    }
}
