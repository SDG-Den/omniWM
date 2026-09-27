# plan phases

this directory is not to be filled until all design phases are complete.


this directory will contain, by the end, 18 markdown files that contain full guides on building the designed system.

building out these files will be done in passes, with each pass adding more detail. 


## pass 1: file creation

pretty simple: create all 18 files with a header that explains the goal of the phase.


## pass 2: components

one file at a time, create headers in the file for each component that needs to be implemented in that phase. the headers should be in a sensible order where no header depends on a header that is later in the file (or in a later file)

## pass 3: component details

one file at a time, go through the headers for what components and fill out the section below with the relevant details to that component, this is done one header at a time

## pass 4: component steps

one file at a time, go through each component and create an overview-level step-by-step, each step is 2-4 phrases, at this point, no code examples have to be given, just a basic description of what needs to happen in this step. each step needs to be ~5-15 minutes for a newbie C developer to reason through. this should be done one component at a time

## pass 5: component step details

go through each file and per component step, add details including markdown links to other sections relevant to the step. 

## pass 6: component step example

provide a full code example for each component step in each file, one at a time, this code example should be split into blocks of 15-25 lines, code should be largely self-explanatory and use human-readable names where possible (no i or c, instead things like index or client, use consistent self-describing names), code must be consistent across component steps, and for this pass, the references created by the previous pass should be used to ensure the code actually works together. 

## pass 7: code descriptions and concepts

create a 19th file: code-concepts.md, go through each component step in each file, and explain any concept being used that would need explaining to a newbie C developer. the explanation should be provided in full the first time it appears, and should also be placed in code-concepts.md, further appearances of the same concept should then just refer to the section in code-concepts.md. 


## how to handle

because this is a *massive* amount of data, it is split into these passes, it is expected that this is done using a read-then-write-then-read-then-write scoped method, as attempting to burst-process it will not go well. 

