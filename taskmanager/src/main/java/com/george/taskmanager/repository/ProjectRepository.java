package com.george.taskmanager.repository;

import com.george.taskmanager.entity.*;

import org.springframework.data.jpa.repository.JpaRepository;

public interface ProjectRepository extends JpaRepository<Project, Long> {}
