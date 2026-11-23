package com.george.taskmanager.dto;

import com.george.taskmanager.entity.TaskStatus;
import jakarta.validation.constraints.Pattern;
import jakarta.validation.constraints.Size;

public record TaskUpdateRequest(
        @Pattern(regexp = "(?s).*\\S.*", message = "title must not be blank")
        @Size(max = 255, message = "title must be at most 255 characters")
        String title,

        @Size(max = 2000, message = "description must be at most 2000 characters")
        String description,

        TaskStatus status
) {
}
