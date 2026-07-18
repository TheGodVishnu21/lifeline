# ---- build stage: compile the backend with gcc on Linux ----
FROM gcc:13 AS build
WORKDIR /app
COPY backend/ backend/
# -static-libstdc++/-static-libgcc: the gcc:13 toolchain's libstdc++ is newer
# than bookworm-slim's, so bundle it into the binary instead of loading at runtime.
RUN cd backend && make LDFLAGS="-pthread -static-libstdc++ -static-libgcc"

# ---- runtime stage: slim image with just the binary + assets ----
FROM debian:bookworm-slim
WORKDIR /app/backend
COPY --from=build /app/backend/lifeline .
COPY backend/data/ data/
COPY frontend-react/dist/ /app/frontend-react/dist/
# Render injects PORT; default 8080 for local runs.
CMD ["sh", "-c", "./lifeline ${PORT:-8080}"]
