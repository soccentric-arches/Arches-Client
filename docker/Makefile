IMAGE_NAME ?= arches

.PHONY: build run clean delete-images

build:
	@echo "Building Docker image $(IMAGE_NAME) for NVIDIA Jetson (ARM64)..."
	docker buildx build --platform linux/arm64 --load --build-arg UID=$(shell id -u) --build-arg GID=$(shell id -g) -t $(IMAGE_NAME) .

run:
	# No --gpus flag: Jetson has CUDA-capable GPU (GPU tests run on-target)
	docker run -it --rm --hostname jetson $(IMAGE_NAME)

clean:
	docker rmi $(IMAGE_NAME) || true

delete-images:
	docker rmi $(docker images -q) || true
