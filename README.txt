imgcmp 1.0

================

An image sorter and deduplicator that does fuzzy image matching via thumbnail pixel comparison, color histogram matching, or other methods.  Maintains an on-disk database for blazing fast matching that makes real-time deduplication as the user saves the image possible.

 - What's the problem?
That damn 4chan keeps filling up my directories with unorganized, and occasionally duplicate images!  For wallpapers especially, this is pretty messy: it'd be nice to keep track of images according to average color, size, filetype too.

 - What can be done to fix it/WTF does this program actually do?
When the file is being saved to a directory, either directly or by the Save Image In Folder plugin, checks to see if the same or a similar image already exists in the said directory.  If so, notifies the user there is/are similar image(s), displays the filenames, and maybe shows the images somehow: via GTK (yuck, complicated), via the browser, or perhaps rely on some other utility such as xv.  It then has the user make a choice, and then cancel the save, or save in another directory.
There is also a batch deduplication feature to clean entire directory structures that have already been created prior to imgcmp's usage.

 - In summary:
	 - Compares a temporary cached image vs. others in a directory
	 - Maintains a cache of thumbs for each image
	 - Deduplicates all images in a directory
	 - TODO:  Communicate back with the browser for user interaction, perhaps, if possible...

	 How does this program check for similarity?
	 - Create a 64x64 thumbnail with reduced color (according to some tolerance setting), add it to a cache in the directory
	   or in a specific location if specified on the command line.
	   - Use a hashtable for exact zero-mismatched-pixel tolerance comparisons
	   - A B+ tree is maintained for very fast lookups of the nearest neighbors in average color; this makes it possible
	     to do real-time deduplication
	   - A much slower but more sensitive "deep scan" will be executed instead if option is set
	 - Use OpenCV's histogram functionality to compare images - Might be thrown off easily by color, but better with details
	   and non-continuous segments. Obviously this creates an additional dependency and might not be any better than the thumbnail
	   method - what if histogram matching were used ON the thumbnails?
	   - Might be able to use edge detection or other image matching techniques that OpenCV can provide
	 - Use ImageMagick to compare images? (icky, additional dependency)
	 - The most accurate method is probably to use pHash

 - Notes:
There is no stored configuration for this utility, all parameters are passed via command line - path, etc.
The configuration is to be stored in the Mozilla plugin, which executes this utility with the appropriate command line.

- Dependencies
	 - libgd for image loading and saving
	 - OpenCV
	 - Mozzarella Foxfire
	 - ImageMagick ?
	 - pHash ?
