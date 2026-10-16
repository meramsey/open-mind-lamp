# Security model and reporting

Open Mind Lamp is a local experimental appliance. Unique WPA2 credentials and an
independent bearer token restrict access; USB physical access is the onboarding
and recovery trust channel. Default HTTP and NVS are not encrypted end-to-end or
at rest. Use trusted local networks; never port-forward the administrative service.
No cloud or update endpoint is contacted by the running lamp.

When a maintainer contact/repository is established, report sensitive issues to
that maintainer privately before disclosing exploit details. This new local
repository does not invent an unmonitored email address or claim a security
response service. Remove all tokens, passwords and personal trial data from
reports. General bugs can use the supplied issue template.
