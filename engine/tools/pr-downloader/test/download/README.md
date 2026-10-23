Download performance testing
============================

This directory contains tools helpful for testing performance of downloading
files using pr-downloader. The original goal was to compare downloading from
rapid repo via streamer vs fetching individual files in parallel.

Setup
-----

The testing setup uses a `gen_repo.py` script to generate a fake rapid repo
where tunables are file size and number of files. The generated repo is then
being served using a container with Nginx as HTTP server.

### Prepare container

NOTE: I'm using [Podman](https://podman.io/) as my container runtime of choice
but it should work in the same way with Docker.

1. Generate self-signed TLS certificate:

   ```
   $ openssl req -x509 -nodes -days 365 -newkey rsa:2048 -keyout nginx-selfsigned.key -out nginx-selfsigned.crt -subj '/CN=localhost'
   ```

2. Build container. I'm using sudo because we need to build a rootfull container
   because we need a routable IP address inside of it.

   ```
   $ sudo podman build -t nginx-tc .
   ```

3. Create directory that will hold rapid repo, here I'm using `repo` in current
   working directory.
4. Start the container

   ```
   $ sudo podman run --rm --name rapid -v "$(pwd)/repo:/srv:ro" --cap-add=NET_ADMIN nginx-tc
   ```

### Set up repository

Now we create a fake rapid repository with a single package `main:pkg:1` in it:

```
$ ./gen_repo.py --num-files=10000 --file-size=102400 https://10.88.0.32
```

as you see, we need to have the IP of Nginx container, we can get it with

```
$ sudo podman container inspect rapid --format '{{.NetworkSettings.IPAddress}}'
```

#### Add real package to repo

We can use the [RapidTools](https://github.com/beyond-all-reason/RapidTools)
`rapid-makezip` and `rapid-addzip` to copy over some package from real
installation to the test repo. Here we will copy `byar:test` package.

1. Temporarily copy `versions.gz` from `rapid/repos.springrts.com/byar/versions.gz`
   to the main root directory in installation, e.g.

   ```
   $ cp ~/Documents/Beyond\ All\ Reason/rapid/repos.springrts.com/byar/versions.gz ~/Documents/Beyond\ All\ Reason
   ```

2. Run `rapid-makezip` to create zip archive of package

   ```
   $ rapid-makezip ~/Documents/Beyond\ All\ Reason/ byar:test byar.zip
   ```

3. Add it to the `main` repository used for testing

   ```
   $ rapid-addzip repo/main ~/byar.zip main:byar:test
   ```

### Set up streamer

Streamer again comes from the RapidTools, you need to place the binary in
the `repo` directory. Then you need to create a symlink to it from the `main`
repo:

```
$ ln -s ../rapid-streamer repo/main/streamer.cgi
```

If there are any issues with it, you will see it in Nginx logs.

Testing
-------

Once everything is in place we can configure networking condition of our
container using `tc` (Traffic Control) utility to conditions under which we want
to test:

```
$ sudo podman exec -it rapid tc qdisc add dev eth0 root netem delay 50ms 10ms 25% rate 50mbps
```

It's a very powerful tool, here I'm just setting up a 50ms delay with some jitter
and set the speed to 50Mbps.

With this in place, we can run pr-dwnloader and measure performance, e.g.:

```
$ time PRD_DISABLE_CERT_CHECK=true PRD_RAPID_REPO_MASTER=https://10.88.0.32/repos.gz PRD_RAPID_USE_STREAMER=false ./src/pr-downloader --filesystem-writepath /tmp/pr-downloader-test --rapid-download main:pkg:1
```
