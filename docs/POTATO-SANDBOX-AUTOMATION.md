# Stop testing by hand: what can be automated, and what cannot

## The complaint, stated exactly

> "maybe you can make a workflow that downloads BlueStacks 5, patches the EXE and
> tests it too much to test manually every time — it already happened with me
> downloading every time and testing, but nothing worked"

Two different problems hide in that sentence:

1. **You re-download a 800 MB–2 GB emulator + player for every attempt.** That is
   avoidable: download once, patch once, hand you the patched executable.
2. **Something boots the game and reports why Potato did nothing.** That is only
   partly avoidable, and the limit has nothing to do with effort.

## Why a cloud workflow cannot run the game for you

BlueStacks 5 is an Android virtual machine. It needs a hypervisor: VT-x/AMD-V on
the host. GitHub's hosted runners do not provide one for guests — their Windows
runners expose no nested virtualisation and their Ubuntu runners have no
`/dev/kvm` ([runner matrix](https://www.warpbuild.com/answers/do-windows-github-actions-runners-support-nested-virtualization),
[KVM in Actions](https://actuated.com/blog/kvm-in-github-actions)). A workflow
that installed BlueStacks on a cloud runner would download two gigabytes, install
it, and then fail to boot the instance — the same "nothing happened" the user is
tired of, just on a server instead of their PC.

So the automation is split honestly:

| Piece | Where | Status |
| --- | --- | --- |
| Download installer, verify digest | `.github/workflows/player-sandbox.yml` | real |
| Build the production patcher and patch the pinned reference | same job, every run | real |
| **Publish the patched `HD-Player.exe`** so the next test is not another download | release tag `sandbox` | real |
| Boot the instance headless, launch a game, collect the status token | only a self-hosted Windows PC with VT-x/AMD-V | real attempt, honest report |
| Boot the instance on a cloud runner | — | **refused by design**, prints `SANDBOX BLOCKED` |

The workflow's capability step reads the machine: on the hosted image it records
`sandbox_capable=false` and the attempt step exits with an explicit notice instead
of a fake pass. Run it with `runner: self-hosted` on a PC with virtualisation
enabled — the same PC you play on — and it does the whole chain unattended:
install, replace the player with the patched copy, start the instance, launch the
game, wait, read the status, classify it, and leave the machine restorable
(`HD-Player.exe.stock`).

`dry_run` (default **true**) validates everything except the download, so the
machinery stays exercised in CI without pulling an installer on every push.

## The status token: why you no longer paste screenshots

Every bounce of "it did nothing" cost a round trip because the diagnosis lived in
a text block only you could read. The app now writes that block itself:

* the same **1-second tick** that refreshes the Shaders UI re-serializes the
  already-published status — no new GL query, no upload, no game content;
* it writes **only when the text changed**, one small JSON file next to its own
  settings (`BSI_SHADERS_STATUS_TOKEN` can name the path for a harness);
* it carries numeric counters plus the trimmed status lines, and nothing that
  identifies the game's resources, windows or paths.

`scripts/Analyze-PotatoStatus.py` then answers the only question worth asking:

```
VERDICT: no_eligible_sampler
  Draws were inspected but every sampler was refused, so no private sampler was bound.
  armed=True inspected=7206 preparedBindings=0
  first blocker: … single-level=2; storage-metadata=0; base-only-filter=1
  next: The bound textures report a single level. No filter change can make a
        coarser image for them; this is a resource-shape answer, not a Potato setting.
```

The classifier never turns a binding count into a claim about pixels, treats a
missing token as an absent measurement (exit 2, not a failure), and names a
pre-repair build by its old `non-mip-filter=` label instead of guessing.

## What is and is not proven

* **Proven by tests here:** the classifier's verdicts, and the writer's contract
  (source-level: on the UI tick, write-on-change, no GL/HTTP/screen access).
  `tests/potato_status_token_test.py` runs in CI.
* **Proven by the sandbox workflow:** the installer digest, the production patch
  of the pinned reference, and the published patched EXE.
* **Not proven anywhere yet:** that Potato changes pixels in *your* game. That
  needs the instance to boot with the game running, which needs a hypervisor.
  The first run that reaches it will report a verdict instead of a screenshot.

BlueStacks itself is never redistributed here: the release carries only BSI's own
patched player executable. Nothing in this workflow bypasses a licence or an
anti-cheat check, and no account or profile is modified beyond the patched
executable that the user already installs manually today.
