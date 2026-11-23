package com.george.taskmanager.repository;

import com.george.taskmanager.entity.Task;
import com.george.taskmanager.entity.TaskStatus;
import org.springframework.data.jpa.repository.JpaRepository;

import java.util.List;

public interface TaskRepository extends JpaRepository<Task, Long> {

    List<Task> findAllByOrderByCreatedAtDesc();

    List<Task> findByStatusOrderByCreatedAtDesc(TaskStatus status);
}
