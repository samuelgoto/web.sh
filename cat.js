
function main(argv) {
  if (argv.length != 2) {
    console.log(`Usage: cat filename`);
    return;
  }
  console.log(`Opening file: ${argv[1]}`);

  requestFileSystem((fs) => {
    // console.log("got fs");

    fs.root.getFile(argv[1], {}, (file) => {
      // console.log("got file");
      // console.log(`filename: ${file.name}`);
      // console.log("foo");
      let a = new FileReader();
      // console.log(a.readAsText);
      a.onloadend = function(e) {
	// console.log("called");
	console.log(this.result);
      }
      a.readAsText(file);
    });
  });
}
