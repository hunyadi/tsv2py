# Parse and generate tab-separated values (TSV) data

This C extension module uses the Python [Limited API](https://docs.python.org/3/c-api/stable.html) targeting Python 3.8 and later.

## Installing the build environment

The following steps demonstrate how to create a development environment suitable for building `tsv2py` from source on Amazon Linux.

### Creating a local build

Building the source code requires a C compiler such as `gcc` or `clang`. The following commands install a C compiler and the Python development headers on AWS Linux:

```sh
sudo yum groupinstall -y "Development Tools"
sudo yum install -y python3-devel python3-pip
```

Install the [Python build front-end](https://build.pypa.io/en/stable/) for creating Python packages:

```sh
python3 -m pip install build
```

### Publishing to PyPI

Docker is required to create packages for PyPI in an isolated environment:

```sh
sudo yum install -y docker
sudo systemctl start docker
sudo systemctl enable docker
sudo usermod -aG docker ec2-user
newgrp docker
```

[cibuildwheel](https://cibuildwheel.readthedocs.io/en/stable/) helps create packages for PyPI that target multiple platforms:

```sh
python3 -m pip install cibuildwheel
```

## Building from source

First, you need to clone the project repository:

```sh
git clone https://github.com/hunyadi/tsv2py.git
```

If you want to create a package locally, simply invoke the build front-end:

```sh
python3 -m build
```

If you would like to create several packages for multiple target platforms, use [cibuildwheel](https://cibuildwheel.readthedocs.io/en/stable/):

```sh
cibuildwheel --platform linux
```

Optionally, scan for possible ABI3 violations and inconsistencies with [abi3audit](https://github.com/trailofbits/abi3audit):

```sh
abi3audit wheelhouse/*-cp3*.whl
```

Finally, use `scp` to copy files from Amazon Linux to a local computer. `1.2.3.4` stands for the IP address of the EC2 machine. `tsv2py.pem` is a certificate to connect to the remote EC2 machine.

```sh
scp -r -i ~/.ssh/tsv2py.pem ec2-user@1.2.3.4:/home/ec2-user/tsv2py/wheelhouse/ wheelhouse
```
