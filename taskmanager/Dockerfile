FROM eclipse-temurin:21-jdk AS build
WORKDIR /build
COPY .mvn .mvn
COPY mvnw pom.xml ./
RUN chmod +x mvnw && ./mvnw -B dependency:go-offline
COPY src src
RUN ./mvnw -B package -DskipTests
FROM eclipse-temurin:21-jre
WORKDIR /app
RUN useradd --system --uid 10001 app && mkdir data && chown app:app data
COPY --from=build /build/target/taskmanager-0.0.1-SNAPSHOT.jar app.jar
USER app
EXPOSE 8080
ENTRYPOINT ["java","-jar","app.jar"]
