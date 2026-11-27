# The `.ogt` file format

A project is a single file. It starts with a four byte header, followed by the
graph encoded as [CBOR](https://cbor.io) and compressed with zlib through
Qt's `qCompress`.

| Offset | Size | Content |
|-------:|-----:|---------|
| 0 | 3 | `OGT` in ASCII |
| 3 | 1 | format version, currently `1` |
| 4 | 4 | unpacked size of the CBOR, big-endian (written by `qCompress`) |
| 8 | ... | zlib stream |

A reader refuses a file whose version is higher than the one it knows, and a
file that claims to unpack to more than 256 MB.

## The graph

Every map uses small integers as keys, and a field equal to its default is
left out. A note with a short title and a position takes about twenty bytes before
compression.

The root map:

| Key | Type | Meaning |
|----:|------|---------|
| 0 | uint | next free neuron id |
| 1 | array of neuron maps | neurons, sorted by id |
| 2 | array of link maps | links, in creation order (absent when there are none) |

A neuron:

| Key | Type | Default | Meaning |
|----:|------|---------|---------|
| 0 | uint | | id, never 0 and never reused |
| 1 | uint | 0 | kind: 0 note, 1 task, 2 web link, 3 image, 4 document, 5 spreadsheet, 6 PDF, 7 other file, 8 folder |
| 2 | text | empty | title |
| 3 | text | empty | body of a note or task, or the address of a web link |
| 4 | text | empty | file path, relative to the folder holding the `.ogt`, with `/` separators |
| 5 | uint | 0 | task status: 0 to do, 1 in progress, 2 done |
| 6 | int | none | due date as a Julian day number |
| 7 | int | 0 | x of the top-left corner, in canvas pixels |
| 8 | int | 0 | y of the top-left corner |
| 9 | bytes | empty | thumbnail, a JPEG (or PNG) at most 160 px on its long side |
| 10 | uint | 0 | id of the folder it sits in, 0 for the top level |
| 11 | uint | 0 | icon: 0 automatic, 1 star, 2 idea, 3 person, 4 flag, 5 home, 6 heart |
| 12 | bytes | empty | custom icon image, compressed and at most 96 px on its long side |

A link:

| Key | Type | Default | Meaning |
|----:|------|---------|---------|
| 0 | uint | | id of the neuron it starts from |
| 1 | uint | | id of the neuron it points to |
| 2 | uint | 0 | kind: 0 related to, 1 leads to, 2 part of |
| 3 | text | empty | label |
| 4 | uint | 0 | group id for branches that join into one final arrow |

Two neurons share at most one link, in either direction, and both ends of a
link sit in the same folder. A link that breaks either rule, or whose ends are
missing, is dropped when the file is read.

## Folders

A folder is a neuron of kind 8. Its content is every neuron whose key 10
holds the folder's id, so the list of neurons stays flat and a folder can
hold other folders. When the file is read, a neuron whose folder is missing,
is not a folder, or belongs to a loop of folders is moved to the top level.

## Changing the format

Keys and enum values are never renumbered. A new field gets the next free
key, and old readers skip keys they do not know. An unknown neuron kind reads
as "other file" and an unknown link kind as "related to", so a project from a
slightly newer version still opens. Only a change that old readers would get
wrong bumps the version byte.

Files are not copied into the project. Moving the project folder keeps every
link working, as long as the files move with it.
