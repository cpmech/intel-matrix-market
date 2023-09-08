# Linear systems with Intel DSS and matrix market

This code solves linear systems with Intel DSS solver and large matrices from Matrix Market

## Installation

### 1 Install Intel MKL

```bash
bash scripts/install-intel-mkl-linux.bash
```

### 2 Download some Matrix Market Files

```bash
bash scripts/download-from-matrix-market.bash
```

### 3 Run Example

```bash
bash ./all.bash
```

## Using Docker

### Use Existent Image from cpmech

Just reopen this folder in a container

![VS Code Remote Development](remote-dev-with-vscode.gif)

### Build the Docker Image

```bash
bash ./scripts/docker-build-image.bash
```
