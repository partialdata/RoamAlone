# AGENTS.md — Van-Life Horror Prototype

## Mission

Build a small, playable, single-player horror prototype that one developer can finish and maintain. The experience centers on the uneasy contrast between exploring a campsite and sheltering inside a van at night.

**Use Unreal Engine the way it is designed to be used. Prefer a finished, understandable gameplay loop over architecture, content volume, or technical novelty.**

This file governs agents working in the game repository. Place it beside the `.uproject` file. Follow more specific instructions in nested `AGENTS.md` files where applicable; explicit user instructions take priority. Class names below describe responsibilities, not a requirement to generate every class immediately.

## Engine and documentation baseline

- Target **Unreal Engine 5.8**, using C++ and Blueprints together.
- Epic's official release announcement confirms UE 5.8 availability. The official documentation search results identified the linked C++, Blueprint, and API references as UE 5.8 documentation when checked on **October 26, 2026**. See the reference directory below.
- Use the project's installed **5.8.x** build as the implementation authority. Read its engine association, build version, enabled plugins, and local headers before selecting APIs. Do not assume a particular hotfix is installed or silently upgrade the project.
- Epic documentation links can follow the current documentation version over time. Select **5.8** in the site version selector and verify the page heading before relying on examples. Do not invent version-specific URLs or claim that a newer page proves 5.8 compatibility.
- The Blueprint API reference explicitly warns that its coverage may be incomplete or out of date. Confirm uncertain nodes in the installed editor and uncertain C++ signatures in the installed engine source.
- Treat the guidance in this file as project decisions. The linked Epic pages document engine behavior; they do not prescribe this game's architecture or scope.

## Unreal MCP and Rider workflow

- **Use Unreal MCP as the preferred connection to the Unreal Editor.** Discover the Unreal tools exposed in the current session and inspect their schemas before use. Do not assume tool names, supported operations, or a working connection merely because the plugins are enabled.
- Before editor changes, use an available read-only MCP operation to confirm the connected project is `D:\unrealProjects\RoamAlone\RoamAlone.uproject` and inspect the active map, PIE state, and relevant actors/assets to the extent supported. Resolve a project mismatch before making changes.
- Prefer supported MCP operations for editor inspection, actor/component configuration, Blueprint and map editing, asset saving, Blueprint compilation, and PIE/testing when those capabilities are exposed. Use the actual tool capabilities; MCP availability alone does not prove it can edit graphs, run tests, or package the game.
- **JetBrains Rider is the project's C++ editor.** Make source, build-rule, and config changes through normal file edits and use Rider for code navigation and debugging when needed. Build the actual Unreal target with the installed toolchain, through Rider or the project's build command. Do not replace Rider, change IDE settings, or require Visual Studio's IDE; Unreal's required compiler/build tools may still be installed separately.
- Keep MCP editor operations and native builds coordinated. Check PIE state before editing assets and stop a task-owned PIE session when required. Preserve unrelated unsaved editor work. Follow the Live Coding restrictions below; reflected layout, inheritance, default subobject, and build configuration changes still require closing the editor, a normal build, and reopening. Reconnect and verify the project after an editor restart.
- Make focused editor changes, inspect the resulting state, and explicitly save the affected project-owned assets/maps through supported editor operations. Compile changed Blueprints and inspect diagnostics where supported. Report changed asset paths and graph/functions; a successful tool request is not proof that an asset saved, compiled, or behaved correctly in PIE.
- If an exposed MCP tool supports executing editor scripts, use only verified installed APIs and keep scripts scoped to the requested work. Do not modify binary packages directly, bypass gameplay validation, or change engine/third-party plugin files to make automation work.
- If Unreal MCP is absent, disconnected, or lacks a required operation, state the specific limitation and continue independent source work. Use another supported editor automation path when available; otherwise provide the exact remaining editor steps under the repository rules below. Never claim MCP, Blueprint compilation, PIE, or packaging validation without actual results. Do not install or reconfigure MCP/plugins solely to work around a missing session connection without authorization.

## Prototype contract and scope

The first milestone contains **one campsite, one stationary van with an accessible interior, one playable night, and one complete ending**. A short, repeatable session should demonstrate exploration, retreat, locking up, tension, and resolution.

Required loop:

1. Start at the campsite with a clear immediate objective.
2. Explore outside in third person using Quinn and template locomotion.
3. Interact with a small number of useful props and the van door.
4. Enter the van and switch to true first person.
5. Close and lock the door, perceive an escalating disturbance, and make at least one meaningful response.
6. Reach the authored ending, then restart reliably.

Use a few authored scares, spatial audio, lighting, and uncertainty. A scripted disturbance is sufficient; do not build an autonomous monster unless the current task requires one. Locking should have observable consequences defined by the authored encounter.

**Outside the milestone:** open-world travel, multiple campsites, real vehicle physics, survival meters, crafting, large inventories, combat, procedural encounters, branching campaigns, multiplayer, elaborate save systems, custom locomotion frameworks, and final character art.

If travel is later requested, first use a short illusion: stationary van, moving scenery/parallax, sound, a fade, or a brief sequence. Do not introduce Chaos Vehicles or a road network to support a transition. Even fake travel can wait until the campsite loop works.

Keep a short backlog for extra ideas. An addition must improve the current loop, have a concrete acceptance check, and justify its maintenance cost. Make sensible local implementation choices without asking about every detail. Surface a scope tradeoff when it materially changes the requested experience.

## Work with the engine

- Start from, or preserve, the standard Third Person template and its working movement/input setup. Use `ACharacter`, `UCharacterMovementComponent`, collision, cameras, and animation assets before writing replacements.
- Use Actors for world objects, Scene Components for transforms, Actor Components for cohesive attachable behavior, UMG for prompts, and Unreal's input and timer systems.
- Use Enhanced Input actions and mapping contexts for movement, look, interaction, and any genuinely distinct mode. Avoid raw key polling and duplicate input bindings during transitions.
- Prefer explicit references assigned in the editor or established during initialization. Direct references and casts are acceptable when the relationship is intentional and stable.
- Use an Unreal interface for a real shared contract, such as different interactable actors. Use delegates or Blueprint Event Dispatchers for actual notifications with multiple consumers. Do not create a global event bus.
- Use timers, overlaps, events, and Timelines where appropriate. Tick is allowed for continuous work such as camera updates; keep it bounded and disable unnecessary ticking. Never repeatedly search the entire world on Tick.
- Keep reusable interaction and lock rules out of Level Blueprints. A tiny level-specific cinematic trigger may remain there; move growing orchestration into a placed actor.
- Do not add GAS, an ECS, a service locator, a dependency injection container, or Lyra's full architecture for this prototype. Adopt a larger engine system only when a current requirement earns its cost.

## Minimal ownership and pragmatic SOLID

Begin with one runtime game module. Add another module or subsystem only for a concrete boundary or lifetime requirement.

| Responsibility | Smallest reasonable owner |
| --- | --- |
| Movement and outside/inside camera mode | Existing player Character and its components |
| Possession, UI input mode, cursor behavior | PlayerController, where needed |
| Default pawn and session rules | GameModeBase |
| Door, lock, and van entry/exit validation | Van Actor or its door component, extracted only when useful |
| Shared interactable contract | One small Unreal interface, when multiple actor types need it |
| Night progression and authored disturbances | One placed night director Actor |
| Weather and time calls into UDS | One project-owned Blueprint integration point |
| Interaction prompt | One UMG widget receiving events |

The director may own a short phase enum and a few timers. Do not build a generic quest framework or a data-driven encounter editor for one night. GameInstance is for data that actually needs to survive world changes, not a home for every manager. Do not add custom GameState or PlayerState classes merely to fill framework slots.

Apply SOLID when it removes a real source of coupling:

- **Single responsibility:** keep door validity rules together; keep prompt presentation separate. Do not split every function into a component.
- **Open/closed:** add a small Blueprint child or presentation event when a real variation appears. Avoid speculative extension hooks.
- **Liskov substitution:** an implementation of the interaction contract must preserve its meaning and failure behavior.
- **Interface segregation:** expose only the small interaction surface consumers need. Avoid a universal interface for doors, weather, inventory, and threats.
- **Dependency inversion:** use an interface at a changing boundary; a known van reference does not require an abstraction layer.

Prefer composition and shallow inheritance. A straightforward branch or enum is often the best solution. Every new abstraction should solve a problem visible in the current task.

## C++ and Blueprint division

| C++ normally owns | Blueprint normally owns |
| --- | --- |
| Stable state, invariants, interaction validation | Asset assignment and actor assembly |
| Door/lock and entry/exit operations | Door Timelines, sound, light, and visual responses |
| Shared contracts and reusable gameplay behavior | Camera tuning and animation integration |
| Logic that needs readable diffs or focused automated tests | UMG layout, scare timing, Sequencer, and UDS calls |
| Measured CPU bottlenecks | Fast iteration on the current night's presentation |

This is guidance, not a quota. A small Blueprint prototype is valid. Move behavior to C++ when its complexity, reuse, testing needs, or measured cost justify doing so. Do not implement the same rule independently in both languages.

- Expose intentional operations such as `TryOpenDoor` and `TrySetLocked`; do not expose writable state that bypasses validation.
- Use `UPROPERTY` and `UFUNCTION` specifiers deliberately. Prefer editable defaults and read-only state where possible; add categories, units, and clamps for useful tuning controls.
- Use `BlueprintImplementableEvent` or `BlueprintNativeEvent` for a real presentation/customization seam. Preserve gameplay invariants in the validating operation.
- Keep Blueprint graphs readable with named functions, useful comments, and clear execution flow. Prefer functions over large repeated graphs or macro libraries.
- Use instance-editable properties for a handful of settings. Introduce Data Assets only when repeated configurations make them useful.
- Assemble greybox models, materials, doors, handles, and window geometry in Unreal Blueprint assets. Keep C++ focused on gameplay logic and necessary gameplay components; do not generate the van's model in its native constructor. A handle belongs to the moving door assembly; any fixed inside switch must be visibly mounted on the wall.

## Player, Quinn, and camera rules

**Quinn is the player placeholder, as explicitly requested by the user.** Reuse Epic's mannequin skeleton, template Animation Blueprint, and available locomotion. Use simple geometry and temporary sounds elsewhere. Do not block gameplay on MetaHumans, custom rigs, motion matching, bespoke IK, or polished entry animations.

- Outside: third-person camera on a Spring Arm with appropriate collision handling.
- Inside: **true first person means an embodied, eye-level view tied to the playable character**, with the body visible when looking down where practical. A close third-person camera or detached inspection camera does not satisfy this requirement.
- Prefer the same possessed Character in both modes. Use a small explicit mode such as `Outside`, `Transitioning`, and `Inside`; restore the previous valid state if a transition cannot complete.
- Keep first-person camera motion stable. Do not directly inherit all head-bone animation shake. Tune the eye offset, look limits, FOV, and body visibility in the actual van.
- Solve head/face clipping with a deliberate local-view mesh/visibility setup. Preserve an appropriate world representation for shadows and reflections. Inspect Epic's native First Person Rendering support if useful; it does not replace collision or entry/exit logic.
- Build the interior to a playable scale and test it with Quinn's capsule early. Change collision or crouch behavior deliberately; never disable all collision to force the character inside.
- For the stationary prototype, keep interior and exterior in the same world. Validate a swept destination or suitable clearance check before moving the character to an entry/exit marker. A short fade is acceptable for a constrained doorway.
- Do not mark a transition complete until the destination, camera, movement, and input setup are valid. Prevent overlapping transitions and repeated interaction from leaving input disabled.
- At doorframes and windows, test third-person camera peeking, first-person wall clipping, low ceilings, fast look movement, and blocked exits. Avoid separate interior maps, duplicate pawns, or render-target windows without a demonstrated need.

## Van door and locking contract

Door state belongs to one gameplay owner. UI, audio, animations, and scares observe it; they do not keep competing lock variables.

For one door, separate open/closed and locked/unlocked state is sufficient, with the invariant **an open door cannot be locked**. Add a transition flag only when animation needs it; avoid a large state-machine framework.

| Request | Required behavior |
| --- | --- |
| Open a closed, unlocked door | Open once and update collision/presentation consistently |
| Open a locked door from outside | Refuse; give clear, brief feedback |
| Lock a closed door | Set locked state once and signal the change |
| Lock an open door | Refuse; prompt the player to close it |
| Unlock from inside | Allow; no key, inventory, or resource requirement for the prototype |
| Exit the van | Require a valid open/unlocked route and a clear destination; leave state unchanged if blocked |
| Repeat a request during transition | Ignore or reject safely without replaying effects or corrupting state |

Provide an obvious inside unlock/open path so the player cannot become trapped by the implementation. Any intentional story confinement must be explicitly authored, communicated, and tested. The player may stay inside while a door is open; door position alone must not switch camera mode.

Use a short interaction trace with distance and obstruction checks. For third person, account for the camera offset so the player cannot interact through walls or beyond plausible character reach. Revalidate the target and state when the action executes. Bind one-shot actions so holding a button does not rapidly toggle the lock.

Do not add lockpicking, individual keys, battery dependencies, or multiple lock types. A scare may react to the lock but must not silently bypass its rules. If an authored event changes the door state, use the same validated operations or a clearly defined, tested exceptional action.

## One night and UDS integration

UDS means **Ultra Dynamic Sky**, with Ultra Dynamic Weather if that asset is installed. It is a third-party product, not an Epic engine feature. Its installed version and documentation must be checked separately from UE 5.8.

- Use UDS for sky and environmental time; use UDW for weather effects when available. Do not build a competing sky/weather system.
- The night director owns narrative progression. It requests environmental changes through a small project-owned Blueprint integration point using the installed vendor API. Do not invent function names or compile against an assumed native UDS class.
- Prefer a placed reference and a few explicit calls over a generic weather-provider framework. Do not scatter UDS casts and property writes across interaction actors.
- Keep third-party source/assets unchanged where possible. Store project presets and customization in project-owned assets; use supported overrides or children where appropriate.
- Choose one authority for advancing time. Do not let both the director and UDS independently advance competing clocks. Use explicit phase events for critical scares rather than exact floating-point time equality.
- Start with one authored night/weather setup. Provide a deterministic debug reset and a way to jump to the phase under test; reset timers and event flags between runs.
- Check rain exclusion/occlusion, weather audio indoors, window visibility, fog, exposure, and the cost of clouds and shadows. Do not assume the van roof automatically blocks every weather effect.
- If UDS is unavailable, continue testing gameplay with fixed engine lighting and clearly report the missing integration. Do not purchase, install, upgrade, or claim UDS validation without the required access and authorization.

Use the [vendor's documentation directory](https://www.ultradynamicsky.com/home) to select documentation matching the installed release. Do not assume the newest online manual matches the project's copy.

## C++ implementation discipline

- Follow Epic's naming and coding standards and existing project style. Use engine types and logging conventions; keep includes explicit and `.generated.h` last in reflected headers.
- Use constructors for defaults and default subobjects, not world-dependent gameplay. Initialize runtime references at the appropriate engine lifecycle stage; clean up timers and external bindings during `EndPlay` where applicable. Call required `Super` implementations.
- Use `CreateDefaultSubobject`, `NewObject`, and `SpawnActor` according to object type and lifetime. Do not allocate or delete UObjects with ordinary `new`/`delete` or use standard shared ownership for them.
- Use reflected `TObjectPtr` fields for persistent strong UObject references, `TWeakObjectPtr` for non-owning caches, and soft references where deferred loading is actually useful. Validate references before use; use the engine's garbage collection model.
- Prefer clear enums and small structs over interacting boolean flags. Log actionable failures without spamming each frame. Use assertions for programmer invariants and safe failure paths for missing optional content.
- Keep runtime dependencies in `.Build.cs` minimal and editor-only dependencies out of packaged runtime code. Do not modify generated code or engine source for ordinary gameplay tasks.
- Use Live Coding for suitable implementation changes on supported setups. After reflected layout, inheritance, default subobject, or build configuration changes, close the editor and perform a normal build/reopen check. Do not trust a successful patch as complete validation.

## Testing and definition of done

Validate the behavior changed, not just compilation. Keep testing proportional: focused automated checks for fragile state rules and repeated regressions; a short manual playthrough for camera feel, atmosphere, and simple visual tuning. Do not build a large test harness for low-risk presentation edits.

Minimum verification for the playable milestone:

- Build the actual project target with the installed UE 5.8 toolchain; compile affected Blueprints and check for new warnings/errors.
- Start PIE from a clean run; complete the whole loop and restart. Repeat PIE to catch stale state, duplicate bindings, and lingering timers.
- Verify valid and invalid door operations, including locking while open, locked outside interaction, inside unlock, rapid repeated input, and interaction through obstructions.
- Enter/exit repeatedly; block the exit marker; test interrupted transitions, camera/body clipping, capsule clearance, and input restoration after UI or sequences.
- Trigger the locked and unlocked encounter responses; ensure critical events fire once and reset correctly. Test ending and restart from each relevant debug phase.
- Test UDS time/weather transitions and indoor weather behavior with the installed version, when available.
- Launch a Development package on the intended target platform. Verify the startup map, cooked asset references, input, UI, weather, ending, and restart without editor-only dependencies.

Automate door invariants and invalid-transition cases when implementing that logic. Use Unreal Automation Tests for suitable native logic and Functional Tests for world interactions when repeatable setup is worthwhile. Tests must reset their state and avoid dependence on execution order.

A task is done when the requested behavior works, relevant checks pass, and any remaining validation gap is stated. Never report an editor test, package, screenshot, or performance result that was not actually obtained.

## Performance and iteration budget

- Establish a named target machine, resolution, scalability preset, and frame-time goal before claiming performance success. A provisional 60 fps target is **16.7 ms per frame**, not a measured result or hardware guarantee.
- Profile the representative campsite and the van interior, including a worst-case weather/scare moment. Use `stat unit`, `stat gpu`, and Unreal Insights as appropriate; measure a standalone/package run as well as editor behavior.
- Watch lighting/shadows, volumetric clouds/fog, foliage, transparency, weather particles, audio, and expensive Blueprint work. Tune the largest measured cost first. Do not enable every rendering feature by default.
- Keep interaction queries bounded, avoid per-frame UMG property bindings for infrequent state changes, and avoid synchronous loading during a scare or transition.
- Do not implement pooling, custom streaming, threading, or broad Blueprint-to-C++ conversion without evidence. One small map does not justify a new open-world pipeline.
- Package early to expose cooking and dependency problems. Preserve a known playable baseline before large asset or rendering changes.

## Repository and Codex agent rules

1. **Inspect before editing.** Read applicable instructions, project structure, `.uproject`, source, relevant Blueprints/assets, and existing validation commands. In an empty workspace, distinguish a proposed setup from an existing project.
2. **Finish the smallest useful slice.** State a brief plan for substantial work, implement the requested behavior, verify it, and report what remains. Do not turn a door task into a framework rewrite.
3. **Use actual engine evidence.** Check official 5.8 docs and installed headers/editor for uncertain behavior. Never invent APIs, asset paths, plugin compatibility, or successful tool results.
4. **Respect user work.** Review the working tree; preserve unrelated changes. Avoid destructive resets, bulk renames, engine/plugin upgrades, and broad formatting changes. Commit or publish only when requested or already authorized.
5. **Treat assets correctly.** `.uasset` and `.umap` files are binary Unreal packages. Edit them through supported editor tools, verified automation, or an authorized engine connection; never write text into them or pretend a C++ edit created a Blueprint asset.
6. **Make editor work reproducible.** When direct editor access is unavailable, complete the source changes possible and provide exact remaining steps: parent class, asset path, component settings, exposed property values, and event/function wiring. Mark the feature partially validated until the editor work is performed.
7. **Keep changes reviewable.** Name the source, config, map, and asset changes. For Blueprint edits, describe the changed behavior and relevant graph/functions; include a diff or screenshot when available. Do not regenerate unrelated assets.
8. **Keep version control clean.** Track source, config, the project descriptor, required content, and project-owned plugins as appropriate. Respect existing ignore/LFS policy; normally exclude `Binaries`, `Intermediate`, `Saved`, and derived caches. Use Unreal tools for asset moves and redirector maintenance; check references and cooking afterward.
9. **Validate honestly.** Distinguish build success, Blueprint compilation, PIE, packaged execution, and manual checks. If the engine, plugin, toolchain, or target hardware is unavailable, state precisely which checks could not run and why.
10. **Communicate outcomes.** Finish with what changed, why, what was checked, and any concrete blocker or remaining editor step. Keep progress updates brief and explain meaningful decisions in plain language.
11. **Protect scope and autonomy.** Make reversible local decisions within the task. Ask only for a consequential missing choice or authorization; continue independent work while awaiting information. Do not introduce purchases, external publication, or new dependencies without authorization.

## Official Epic reference directory

Verified against official Epic documentation results on **October 26, 2026**. Recheck the **5.8** selector and installed APIs when using these links. The release notes were identified in official search results; full-page retrieval exceeded the browsing tool's size limit. No particular 5.8 hotfix is asserted here.

| Reference | Use it for |
| --- | --- |
| [UE 5.8 release announcement](https://www.unrealengine.com/news/unreal-engine-5-8-is-now-available) | Confirm the release baseline |
| [UE 5.8 release notes](https://dev.epicgames.com/documentation/unreal-engine/unreal-engine-5-8-release-notes) | Version changes, feature status, and migration details |
| [Unreal Engine documentation](https://dev.epicgames.com/documentation/en-us/unreal-engine) | Documentation entry point and version selection |
| [Programming with C++](https://dev.epicgames.com/documentation/en-us/unreal-engine/programming-with-cplusplus-in-unreal-engine) | Native gameplay development |
| [C++ API reference](https://dev.epicgames.com/documentation/en-us/unreal-engine/API) | Classes, signatures, headers, and modules |
| [Blueprints Visual Scripting overview](https://dev.epicgames.com/documentation/unreal-engine/overview-of-blueprints-visual-scripting-in-unreal-engine?lang=en-US) | Blueprint classes and engine integration |
| [Blueprint API reference](https://dev.epicgames.com/documentation/unreal-engine/BlueprintAPI?lang=en-US) | Node reference, subject to documented coverage limitations |
| [Coding in UE: Blueprint vs. C++](https://dev.epicgames.com/documentation/en-us/unreal-engine/coding-in-unreal-engine-blueprint-vs-cplusplus) | Choosing and combining implementation tools |
| [Blueprint Best Practices](https://dev.epicgames.com/documentation/en-us/unreal-engine/blueprint-best-practices-in-unreal-engine) | Event-driven graphs, functions, and communication |
| [Gameplay Framework](https://dev.epicgames.com/documentation/en-us/unreal-engine/gameplay-framework-in-unreal-engine) | Ownership, possession, and framework lifetimes |
| [Epic C++ Coding Standard](https://dev.epicgames.com/documentation/unreal-engine/epic-cplusplus-coding-standard-for-unreal-engine?lang=en-US) | Naming, readability, and native code conventions |
| [Object Pointers](https://dev.epicgames.com/documentation/en-us/unreal-engine/object-pointers-in-unreal-engine) | UObject references and garbage collection |
| [Actor Lifecycle](https://dev.epicgames.com/documentation/en-us/unreal-engine/unreal-engine-actor-lifecycle) | Initialization and teardown |
| [Enhanced Input](https://dev.epicgames.com/documentation/en-us/unreal-engine/enhanced-input-in-unreal-engine) | Actions, mapping contexts, and triggers |
| [Third Person Template](https://dev.epicgames.com/documentation/unreal-engine/third-person-template-in-unreal-engine?lang=en-US) | Starting character, camera, and mannequin assets |
| [First Person Template](https://dev.epicgames.com/documentation/en-us/unreal-engine/first-person-template-in-unreal-engine) | Native first-person setup examples |
| [First Person Rendering](https://dev.epicgames.com/documentation/en-us/unreal-engine/first-person-rendering) | First-person primitives, world representation, and rendering limits |
| [Live Coding](https://dev.epicgames.com/documentation/unreal-engine/using-live-coding-to-recompile-unreal-engine-applications-at-runtime) | Supported runtime code iteration |
| [Automation Test Framework](https://dev.epicgames.com/documentation/en-us/unreal-engine/automation-test-framework-in-unreal-engine) | Native and functional testing entry points |
| [Performance Profiling and Configuration](https://dev.epicgames.com/documentation/en-us/unreal-engine/introduction-to-performance-profiling-and-configuration-in-unreal-engine) | Frame budgets and profiling approach |
| [Trace Quick Start Guide](https://dev.epicgames.com/documentation/en-us/unreal-engine/trace-quick-start-guide-in-unreal-engine) | Capturing and inspecting traces with Unreal Insights |
| [Packaging Unreal Engine Projects](https://dev.epicgames.com/documentation/unreal-engine/packaging-your-project) | Cooking, startup maps, and standalone build validation |

**Decision test:** Does this help one developer finish and reliably play the one-night prototype? If it does not, defer it.
