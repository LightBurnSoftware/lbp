# Security Policy

The LightBurn Protocol may be used in systems connected to lasers, CNC machinery, motion systems, and other hardware capable of causing physical damage or injury if implemented incorrectly.
Because of that, security and safety are closely related concerns within this ecosystem.

## Reporting Issues

If you discover a vulnerability, unsafe protocol behavior, or implementation issue that could negatively impact users, devices, or machine safety, we encourage responsible disclosure.
Examples may include:
*   unauthorized machine control
*   unsafe motion behavior
*   malformed packet handling
*   denial of service conditions
*   authentication weaknesses
*   unexpected laser activation
*   protocol abuse vectors
*   firmware interaction vulnerabilities

When reporting issues, it is helpful to include as much relevant information as possible, including affected versions, reproduction steps, logs, packet captures, or proof-of-concept examples where appropriate.

## Responsible Disclosure

We ask that serious vulnerabilities not be publicly disclosed immediately before maintainers and affected implementers have had a reasonable opportunity to investigate and respond.
Not every report will result in an immediate fix or public advisory, but all reasonable reports will be reviewed in good faith.

## Scope

This policy applies to official protocol specifications, reference implementations, and tooling maintained directly by the project.
Third-party implementations remain responsible for their own security practices, validation, patch management, and operational safety.

## Safety Expectations
Implementers are strongly encouraged to design around safe failure behavior whenever possible.
That includes things like:
*   explicit state handling
*   defensive parsing
*   timeout handling
*   watchdog behavior
*   hardware interlocks
*   emergency stop integrity
*   avoiding unsafe default states

Security should not be treated as separate from operational safety in machine-control environments.

## No Guarantee

While we will make reasonable efforts to review reported issues, LightBurn Software does not guarantee response timelines, patch availability, compatibility preservation, or support obligations.
Anyone deploying systems based on the protocol remains responsible for validating the safety and security of their own implementation before use in production or real-world environments.
