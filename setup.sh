#!/bin/bash

# Met à jour la liste des paquets et installe les dépendances système
sudo apt-get update
sudo apt-get install -y build-essential libc6-dev

# Compiler le projet
make
