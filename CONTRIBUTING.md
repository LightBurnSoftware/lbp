# Contributing to LightBurn Protocol
Thanks for your interest in contributing to the LightBurn Protocol project.
The protocol is intended to support a broad ecosystem of software, firmware, controllers, and related tooling, and thoughtful community contributions can play a big role in helping it mature into something stable, predictable, and genuinely useful across different environments.
Contributions do not need to be large to be valuable. Documentation improvements, clarification of ambiguous behavior, implementation feedback, testing tools, and compatibility fixes are all meaningful contributions.

## General Approach
When contributing, we encourage people to think beyond whether something merely “works” and consider whether it improves interoperability, predictability, maintainability, and long-term ecosystem stability.
Because this protocol may be used in machine-control environments involving lasers, CNC systems, and motion hardware, clarity and reliability matter a great deal. Small ambiguities can become large support or safety problems later.

## Protocol Changes
Changes that affect protocol behavior should ideally include enough context for others to understand:
*   what problem is being solved
*   whether compatibility is affected
*   whether behavior changes are intentional
*   whether migration concerns exist
*   whether there are any safety implications

Not every proposal will be accepted, especially if it introduces unnecessary complexity, fragmentation, or behavior that may create interoperability problems later.

## Backward Compatibility
Where practical, maintaining backward compatibility is strongly preferred.
Breaking changes are sometimes unavoidable, but they should be approached carefully and with consideration for existing implementations already deployed in the ecosystem.

## Safety Considerations
Implementers and contributors should keep in mind that protocol decisions may ultimately influence physical machine behavior.
Features or changes that create ambiguous states, unpredictable behavior, or unsafe assumptions may be rejected even if they are technically functional.
Reliability and predictability are generally more valuable than cleverness.

## Licensing
Unless explicitly stated otherwise, contributions submitted to this project are understood to be provided under the same MIT License as the rest of the project.
By submitting a contribution, you represent that you have the right to contribute the material in question.

## Conduct
We want the project to remain productive, technically focused, and collaborative.
Disruptive behavior, harassment, intentional ecosystem fragmentation, or bad-faith participation may result in contributions being declined or participation being restricted.

## Security Issues
If you discover a security vulnerability or unsafe behavior, please avoid immediately publishing exploit details publicly before there has been an opportunity to investigate responsibly.
See [SECURITY.md](./SECURITY.md) for additional guidance.
