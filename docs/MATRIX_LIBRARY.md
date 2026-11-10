# Matrix library ownership and changes

The led-matrix owner (`sshcrack`) also owns and maintains
[`sshcrack/rpi-rgb-led-matrix`](https://github.com/sshcrack/rpi-rgb-led-matrix),
the fork used by this application. It is a maintained dependency we can change
when led-matrix needs a library feature or bug fix.

## Shipping a required library change

Use the equivalent of the led-matrix tier 3 workflow for the library:

1. Read the fork's contributor instructions and release workflows. Track the
   library change in its repository, linking the led-matrix issue or PR that
   needs it.
2. Implement the change on a library branch, add relevant regression coverage,
   and open a PR. Verify affected hardware, emulator/headless and Windows paths
   through the fork's supported builds and CI. Report physical-panel behavior
   separately from GPIO-free tests.
3. Resolve failures, merge the green library PR, and publish a versioned library
   release using the fork's release process. Verify that the release exists
   and its required checks and artifacts succeeded.
4. Update led-matrix's dependency pin to that published release, rebuild the
   affected application presets, and verify the integration. Include the library
   PR and release links in the led-matrix PR.

Keep library implementation and library-specific regressions in the fork;
consume its published release here instead of carrying source patches or pinning
an unpublished development branch. Any temporary local override used during
development must be removed from the final application change.

The library release completes the dependency's delivery. The led-matrix PR still
follows the application's requested shipping tier: tier 1 stops at a green PR,
tier 2 merges it, and tier 3 releases and deploys the application to the Pi.
Releasing the library alone does not deploy anything to the Pi.
