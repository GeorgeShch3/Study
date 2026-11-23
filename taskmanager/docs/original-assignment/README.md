# Task Manager

REST API для управления задачами: Spring Boot, Spring Data JPA, H2.

## Требования

Java 17 или новее. Maven ставить не нужно — он скачается через `mvnw`.

## Запуск

```bash
./mvnw spring-boot:run
```

Windows: `mvnw.cmd spring-boot:run`

Приложение запустится на http://localhost:8080.

## Сборка

```bash
./mvnw clean package
java -jar target/taskmanager-0.0.1-SNAPSHOT.jar
```

## Проверка через Swagger

Откройте http://localhost:8080/swagger-ui.html, выберите запрос, нажмите **Try it out**, затем **Execute**.

## База данных

H2 хранится в файле `data/taskdb`. Консоль: http://localhost:8080/h2-console
(JDBC URL `jdbc:h2:file:./data/taskdb`, пользователь `sa`, пароль пустой).
