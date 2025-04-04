[![Build Status][build-badge]][build-link]
[![GitHub Release][release-badge]][release-link]
[![License: LGPL v3][license-badge]](LICENSE.md)
[![Coverage Status][codecov-badge]][codecov-link]
[![CodeFactor Badge][codefactor-badge]][codefactor-link]
[![CII Best Practices][cii-badge]][cii-link]
[![Slack][slack-badge]][slack-link]

<p align="center">
  <a href="https://wrench-project.org" target="_blank"><img src="https://wrench-project.org/images/logo-horizontal.png" width="500" alt="WRENCH Project" /></a>
  <br /><i>Cyberinfrastructure Simulation Workbench</i>
  <br /><br />
</p>

WRENCH is an _open-source library_ that provides high-level simulation 
abstractions that serve as **building blocks** for developing custom simulators of
distributed computing systems, platforms, and applications.

More information and Documentation: [WRENCH Project Website](http://wrench-project.org)

**WRENCH Research Paper:**
- H. Casanova, R. Ferreira da Silva, R. Tanaka, S. Pandey, G. Jethwani, W. Koch, S.
  Albrecht, J. Oeth, and F. Suter, [Developing Accurate and Scalable Simulators of
  Production Workflow Management Systems with 
  WRENCH](https://rafaelsilva.com/files/publications/casanova2020fgcs.pdf), 
  Future Generation Computer Systems, vol. 112, p. 162-175, 2020. 

## Prerequisites

WRENCH is fully developed in C++. The code follows the **C++17** standard, and thus 
older compilers tend to fail the compilation process. Therefore, we strongly 
recommend users to satisfy the following requirements:

- **CMake** - version 3.10 or higher
  
And, one of the following:
- **g++** - version 7.5 or higher
- **clang** - version 9.0 or higher

WRENCH also comes with a [REST API](https://wrench-project.org/wrench/latest/restapi/index.html).  A
Python API, built
on top of the REST API, is available in [a separate repository](https://github.com/wrench-project/wrench-python-api).

## Dependencies

### Required Dependencies 

- [SimGrid](https://framagit.org/simgrid/simgrid/) - version 4.0
- [SimGrid File System Module](https://github.com/simgrid/file-system-module) - version 0.3 or higher
- [JSON for Modern C++](https://github.com/nlohmann/json) - version 3.11.0 or higher 

### Optional Dependencies

- [Google Test](https://github.com/google/googletest) - version 1.8 or higher (only required for running test cases)
- [Doxygen](http://www.doxygen.org) - version 1.8 or higher (only required for generating documentation)
- [Swagger-codegen](https://github.com/swagger-api/swagger-codegen) - version 3.0 or higher (only required for generating documentation)
- [Sphinx](https://www.sphinx-doc.org/en/master/usage/installation.html) - version 4.5 or higher along with the following Python packages: `pip3 install sphinx-rtd-theme breathe recommonmark`  (only required for generating documentation)
- [Batsched](https://gitlab.inria.fr/batsim/batsched) - version 1.4 - useful for expanded batch-scheduled resource simulation capabilities
- [Asio](https://github.com/chriskohlhoff/asio/) - necessary only for building `wrench-daemon`

## Building From Source

If all dependencies are installed, compiling and installing WRENCH is as simple as running:

```bash
mkdir build
cd build
cmake ..
make
sudo make install
```

For enabling the use of Batsched:
```bash
mkdir build
cd build
cmake -DENABLE_BATSCHED=on ..
make
sudo make install
```

To use a non-standard SimGrid installation path:
```bash
mkdir build
cd build
cmake -DSimGrid_PATH=/my/simgrid/path/ ..
make
sudo make install 
```

## Get in Touch

The main channel to reach the WRENCH team is via the support email: 
[support@wrench-project.org](mailto:support@wrench-project.org).

**Bug Report / Feature Request:** our preferred channel to report a bug or request a feature is via  
WRENCH's [Github Issues Track](https://github.com/wrench-project/wrench/issues).

## Citing WRENCH

When citing WRENCH, please use the following paper. You should also actually read that paper, as 
it provides a recent and general overview on the framework.

```latex
@article{wrench,
  title = {Developing Accurate and Scalable Simulators of Production Workflow Management Systems with WRENCH},
  author = {Casanova, Henri and Ferreira da Silva, Rafael and Tanaka, Ryan and Pandey, Suraj and Jethwani, Gautam and Koch, William and Albrecht, Spencer and Oeth, James and Suter, Fr\'{e}d\'{e}ric},
  journal = {Future Generation Computer Systems},
  volume = {112},
  number = {},
  pages = {162--175},
  year = {2020},
  doi = {10.1016/j.future.2020.05.030}
}
```

## List of Contributors via Pull Requests

| User | Description | 
| --- | --- |
| [@dohoangdzung](https://github.com/dohoangdzung) | I/O with page cache simulation model (09/2020) |

## Funding Support

WRENCH has been funded by the National Science Foundation (NSF), and the National Center for Scientific Research (CNRS).

[![NSF Funding 2103489][nsf-2103489-badge]][nsf-2103489-link]
[![NSF Funding 2103508][nsf-2103508-badge]][nsf-2103508-link]
[![NSF Funding 20191][nsf-20191-badge]][nsf-20191-link]
[![NSF Funding 20192][nsf-20192-badge]][nsf-20192-link]
[![NSF Funding 20161][nsf-20161-badge]][nsf-20161-link]
[![NSF Funding 20162][nsf-20162-badge]][nsf-20162-link]
![CNRS Funding 2015][cnrs-2015-badge]

[build-badge]:              https://github.com/wrench-project/wrench/actions/workflows/build.yml/badge.svg
[build-link]:               https://github.com/wrench-project/wrench/actions/workflows/build.yml
[build-macos-badge]:        https://github.com/wrench-project/wrench/actions/workflows/build-macos.yml/badge.svg
[build-macos-link]:         https://github.com/wrench-project/wrench/actions/workflows/build-macos.yml
[license-badge]:            https://img.shields.io/badge/License-LGPL%20v3-blue.svg
[codecov-badge]:            https://codecov.io/gh/wrench-project/wrench/branch/master/graph/badge.svg
[codecov-link]:             https://codecov.io/gh/wrench-project/wrench
[coveralls-badge]:          https://coveralls.io/repos/github/wrench-project/wrench/badge.svg?branch=master
[coveralls-link]:           https://coveralls.io/github/wrench-project/wrench?branch=master
[release-badge]:            https://img.shields.io/github/release/wrench-project/wrench/all.svg
[release-link]:             https://github.com/wrench-project/wrench/releases
[codefactor-badge]:         https://www.codefactor.io/repository/github/wrench-project/wrench/badge/master
[codefactor-link]:          https://www.codefactor.io/repository/github/wrench-project/wrench/overview/master
[cii-badge]:                https://bestpractices.coreinfrastructure.org/projects/2357/badge
[cii-link]:                 https://bestpractices.coreinfrastructure.org/projects/2357
[slack-badge]:              https://img.shields.io/badge/slack-%40wrench--project-yellow?logo=slack
[slack-link]:               https://join.slack.com/t/wrench-project/shared_invite/zt-g88hfxj7-pcuxOBMS7jSBkq1EhAn2~Q
[nsf-2103489-badge]:        https://img.shields.io/badge/NSF-2103489-blue
[nsf-2103489-link]:         https://nsf.gov/awardsearch/showAward?AWD_ID=2103489
[nsf-2103508-badge]:        https://img.shields.io/badge/NSF-2103508-blue
[nsf-2103508-link]:         https://nsf.gov/awardsearch/showAward?AWD_ID=2103508
[nsf-20191-badge]:          https://img.shields.io/badge/NSF-1923539-blue
[nsf-20191-link]:           https://nsf.gov/awardsearch/showAward?AWD_ID=1923539
[nsf-20192-badge]:          https://img.shields.io/badge/NSF-1923621-blue
[nsf-20192-link]:           https://nsf.gov/awardsearch/showAward?AWD_ID=1923621
[nsf-20161-badge]:          https://img.shields.io/badge/NSF-1642369-blue
[nsf-20161-link]:           https://nsf.gov/awardsearch/showAward?AWD_ID=1642369
[nsf-20162-badge]:          https://img.shields.io/badge/NSF-1642335-blue
[nsf-20162-link]:           https://nsf.gov/awardsearch/showAward?AWD_ID=1642335
[cnrs-2015-badge]:          https://img.shields.io/badge/CNRS-PICS07239-blue
