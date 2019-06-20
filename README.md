[![Build Status](https://travis-ci.org/TextpressoDevelopers/libtpc.svg?branch=master)](https://travis-ci.org/TextpressoDevelopers/libtpc) [![Documentation Status](https://readthedocs.org/projects/libtpc/badge/?version=latest)](http://libtpc.readthedocs.io/en/latest/?badge=latest)

# Libtpc
## Description
Textpresso Central is a platform to perform full text literature searches, view and curate research papers,
train and apply machine learning (ML) and text mining (TM) algorithm for semantic analysis and curation purposes.
The user is supported in this task by giving him capabilities to select, edit and store lists of papers, sentences,
term and categories in order to perform training and mining. The system is designed with the intent to empower the user
to perform as many operations on a literature corpus or a particular paper as possible. It uses state-of-the-art
software packages and frameworks such as the Unstructured Information Management Architecture (UIMA), Lucene and Wt.
The corpus of papers can be build from fulltext articles that are available in PDF or NXML format.

libtpc is the core library of Textpresso Central, and includes functions to convert documents, annotate, index and
search them.

## Installation
### Dependencies
To compile libtpc, the following libraries and programs are needed:

* cmake
* lucene++
* xerces-c
* apr
* aprutil
* icu
* boost
* podofo
* pqxx
* lighttp
* fcgi++
* magick
* wt
* curl
* eigen3
* postgresql
* cimg
* GTest
* bzip2
* wget
* unzip
* python-software-properties
* autoconf
* git
* uima

---
**NOTE**

cmake version >= 3.5 is required.

---

### Compile and Install libtpc
To compile and install libtpc, run the following commands from the root directory of the repository:
```{r, engine='bash', count_lines}
$ mkdir cmake-build-release && cd cmake-build-release
$ cmake -DCMAKE_BUILD_TYPE=Release ..
$ make && make install
```

This will install libtpc in its default location (/usr/local/lib).

### Debug mode
libtpc can be also compiled and installed in debug mode, with the following commands:
```{r, engine='bash', count_lines}
$ mkdir cmake-build-debug && cd cmake-build-debug
$ cmake -DCMAKE_BUILD_TYPE=Debug ..
$ make && make install
```

### Install Textpressocentral Database
#### Create postgresql users and table
The users "www-data" and "textpresso" must be created, along with "www-data" and "textpresso" databases.
To do so, install postgresql-client, and set "trust" method for local access to all users in
/etc/postgresql/9.6/main/pg_hba.conf. Then, run the following commands to create the users:

```{r, engine='bash', count_lines}
$ sudo -u postgres createuser "www-data"
$ sudo -u postgres createuser textpresso
```

Enter the psql console by running:
```{r, engine='bash', count_lines}
$ sudo -u postgres psql
```

Inside the console, run the following commands to create the databases:
```{r, engine='bash', count_lines}
postgres=# CREATE DATABASE "www-data";
postgres=# CREATE DATABASE "textpresso";
```

---
**NOTE**

If you want to debug libtpc, create a user and a database with your username
(the user that will launch the server instance) and grant all privileges on all tables, by running the
following command in the psql console launched as user postgres:
```{r, engine='bash', count_lines}
postgres=# GRANT ALL PRIVILEGES ON ALL TABLES IN SCHEMA public TO "<your_username>";
postgres=# GRANT ALL PRIVILEGES ON ALL SEQUENCES IN SCHEMA public TO "<your_username>";
```

---

#### Populate the Database

The easiest way to populate the database is to restore it from a dump file, with the following command:
```{r, engine='bash', count_lines}
sudo -u postgres pg_restore -U postgres -d www-data www-data.20170523.tar
```

## Textpresso Central Docker images

### Use a pre-built image
A Docker image based on Ubuntu 16.04 and with all the libraries required to compile and run libtpc and the other 
Textpresso projects (textpressocentral and tpctools) is vailable on Docker hub. To pull it, run the following command:
```{r, engine='bash', count_lines}
$ sudo docker pull valearna/ubuntu-tpc-buildtest:16.04
```

To run the image and connect to an interactive shell:
```{r, engine='bash', count_lines}
$ sudo docker run -p80:80 -it valearna/ubuntu-tpc-buildtest:16.04
```

libtpc and the other Textpresso projects can be directly compiled and installed on the image.

### Create a local image
The repository contains a Dockerfile that can be used to generate a Docker image: Dockerfile-tpc

To build the image, run the following command:
```{r, engine='bash', count_lines}
$ sudo docker build -t <image_name>:<tag> -f Dockerfile <repository_root>
```

---

**NOTE**
The image comes with libtpc pre-installed, but the other Textpresso project have to be manually installed from its 
console.

---

### Connect to a running image and get a shell prompt
To connect to a running Docker container, execute the following command:
```{r, engine='bash', count_lines}
$ sudo docker exec -i -t <container_name_or_id> /bin/bash
```


## Further steps for manual installation
### Load literature data
After installing Textpressocentral, the literature data must be populated. To do so, copy the LuceneIndex folder from an
existing Textpressocentral installation. For example, to copy the C. Elegans literature, run the following commands:
```{r, engine='bash', count_lines}
$ sudo scp -r <username>@<host>:/usr/local/textpresso/luceneindex/C.\\\ elegans_0/ /usr/local/textpresso/luceneindex/
$ scp -r <username>@<host>:/usr/local/textpresso/tpcas/C.\\\ elegans /usr/local/textpresso/tpcas/
$ sudo echo "C. elegans" > /usr/local/textpresso/luceneindex/subindex.config
$ sudo ln -s /usr/lib/cgi-bin/tc/images /usr/local/textpresso/tpcas/
```


