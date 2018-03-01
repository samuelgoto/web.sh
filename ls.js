function main(argv) {
  requestFileSystem((fs) => {
    fs.root.createReader(1).readEntries((directory) => {
      directory.forEach((file) => {
	console.log(`${file.name}${file.isDirectory ? "/" : ""}`)
      })
    })
  })
}
