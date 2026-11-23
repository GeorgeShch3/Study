package com.george.taskmanager.exception;

import org.springframework.dao.DataIntegrityViolationException;
import org.springframework.http.*;
import org.springframework.orm.ObjectOptimisticLockingFailureException;
import org.springframework.web.bind.MethodArgumentNotValidException;
import org.springframework.web.bind.annotation.*;
import org.springframework.web.context.request.WebRequest;
import org.springframework.web.servlet.mvc.method.annotation.ResponseEntityExceptionHandler;

import java.util.LinkedHashMap;

@RestControllerAdvice
public class GlobalExceptionHandler extends ResponseEntityExceptionHandler {
    @ExceptionHandler(ApiException.class)
    ProblemDetail api(ApiException e) {
        return ProblemDetail.forStatusAndDetail(e.getStatus(), e.getMessage());
    }

    @ExceptionHandler(ObjectOptimisticLockingFailureException.class)
    ProblemDetail conflict(ObjectOptimisticLockingFailureException e) {
        return ProblemDetail.forStatusAndDetail(
                HttpStatus.CONFLICT, "Task changed. Reload it and retry with the latest version.");
    }

    @ExceptionHandler(DataIntegrityViolationException.class)
    ProblemDetail integrity(DataIntegrityViolationException e) {
        return ProblemDetail.forStatusAndDetail(
                HttpStatus.CONFLICT, "A record with these unique attributes already exists.");
    }

    @Override
    protected ResponseEntity<Object> handleMethodArgumentNotValid(
            MethodArgumentNotValidException ex,
            HttpHeaders headers,
            HttpStatusCode status,
            WebRequest request) {
        var errors = new LinkedHashMap<String, String>();
        ex.getBindingResult()
                .getFieldErrors()
                .forEach(
                        e ->
                                errors.merge(
                                        e.getField(),
                                        e.getDefaultMessage(),
                                        (a, b) -> a + "; " + b));
        var body = ex.getBody();
        body.setProperty("errors", errors);
        return handleExceptionInternal(ex, body, headers, status, request);
    }
}
