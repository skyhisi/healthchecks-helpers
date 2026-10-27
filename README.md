# healthchecks-helpers
Helper utilities for healthchecks.io monitoring

```
HEALTHCHECKS-SYSTEMD-ONESHOT(1)       1.0      HEALTHCHECKS-SYSTEMD-ONESHOT(1)

NAME
       healthchecks-systemd-oneshot  -  ping  Healthchecks.io  from  a systemd
       oneshot service

SYNOPSIS
       healthchecks-systemd-oneshot [-jvh] URL

DESCRIPTION
       healthchecks-systemd-oneshot reports the outcome of a  systemd  oneshot
       service to Healthchecks.io ⟨https://healthchecks.io⟩.

       It  is  intended to be invoked from ExecStartPost= (to signal the start
       of the run) and ExecStopPost= (to signal success or failure).

OPTIONS
       -j     For ExecStopPost= attach the service's journal entries.

       -v     Verbose logging.

       -h     Print usage.

ENVIRONMENT
       SERVICE_RESULT
              Set by systemd for ExecStopPost=; its presence distinguishes the
              final ping from the start ping.

       EXIT_CODE
              When exited, the run is reported with EXIT_STATUS.

       EXIT_STATUS
              Process exit status; must parse as a number in 0–255.

       INVOCATION_ID
              Used to build the rid= parameter and to  match  journal  entries
              for -j.

EXIT STATUS
       The exit status is the libcurl result code: 0 on success, non-zero oth‐
       erwise  (e.g.  7 couldn't connect, 22 HTTP response >= 400 because CUR‐
       LOPT_FAILONERROR is set).

EXAMPLES
       Start and result pings for a oneshot service:

       [Service]
       Type=oneshot
       ExecStart=/usr/bin/myjob
       ExecStartPost=healthchecks-systemd-oneshot https://hc-ping.com/<uuid>
       ExecStopPost=healthchecks-systemd-oneshot -j https://hc-ping.com/<uuid>

SEE ALSO
       Healthchecks.io HTTP API ⟨https://healthchecks.io/docs/http_api/⟩, sys‐
       temd.service(5), systemd.exec(5)

healthchecks-helpers              2026-10-27   HEALTHCHECKS-SYSTEMD-ONESHOT(1)
```
