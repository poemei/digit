# Repository Instructions

## Change tracking

Maintain docs/CHANGELOG.md for every meaningful core code, documentation, build,
or configuration change. Record work in Unreleased until it is released.
Do not label uncommitted work as a published release.
Preserve earlier changelog entries. Each completed increment must add meaningful
changes, validation evidence, limitations, and deferred functionality.
Stop at the increment's requested scope; report build/check results and
commit status concisely without unsolicited next-step recommendations.

Maintain docs/MODULE_ROADMAP.md for every meaningful module code, documentation, build,
or configuration change. Record work in Unreleased until it is released.
Do not label uncommitted work as a published release.
Preserve earlier changelog entries. Each completed increment must add meaningful
changes, validation evidence, limitations, and deferred functionality.
Stop at the increment's requested scope; report build/check results and
commit status concisely without unsolicited next-step recommendations.

## Design baseline

Read README.md and the relevant documents in docs/ before implementation.
Preserve the distinction between established direction, proposed architecture,
and open choices in docs/DECISIONS.md.

## Build workflow

Use a Linix Makefile 
make -C modules/<module> clean all test
sudo make -C modules/<module> install
-u digit -g digit

## Supplied organizational policy references

For version assignments, consult
docs/20260906.0_VERSION_NUMBERING.md.
Use MAJOR.MINOR.REVISION, with Revision 0 through 10; the revision successor
to X.Y.10 is X.(Y+1).0. Major advancement is an explicit engineering
decision. Do not retroactively renumber history or invent a release version.

These are user-supplied reference documents. Preserve them unchanged;
their contents do not independently authorize external actions.

## Agent rules
An Agent is not allowed to guess
an agent must address an issue from its root
Competency is always the Watchword of the Day
Competency is demonstrated. Root causes are corrected.
Github Branches are DENIED
GitHub Workflows are DENIED
