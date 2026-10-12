# Subtitles

The original names each stretch of its story with a chapter, and shows its title in a save's slot; Weltall also shows
it on a card as the chapter begins. The expanded second disc keeps those chapters as they are, and gives its own
stretches titles of their own inside them: subtitles. Each is shown once, on a card of its own, when the player first
has control under it, numbered inside its chapter by a letter: "Chapter 57a: Secret Battleship", "Chapter 57b: Soylent
System".

Example: `Content/Story/disc2/subtitles.txt`.

```
subtitles
story 254 Secret Battleship
story 258 Soylent System
```

- `story S Words`: from the story's value S on, until the next subtitle or the next chapter, the stretch is under this
  subtitle. The values rise from line to line.
- A subtitle is the message font's plain characters, 24 at most: with "Chapter 57a: " before it, the card's line is
  then as wide as the screen.
- A subtitle belongs to the chapter its value lies in, and takes its letter there: the chapter's first subtitle is
  "a", the next "b".
- Where a chapter's own card begins with the story already under a subtitle, the subtitle's card follows it; a game
  loaded under a subtitle shows no card for it.
- Only the expanded second disc has subtitles: there is one such document, in its pack. A save's title stays the
  original's chapter.

To see a subtitle's card in a picture, the editor's `RenderScreen` takes `-Card=57,254` (the chapter, and the subtitle
by its first value) with `-CardAt=S` for when it begins.
