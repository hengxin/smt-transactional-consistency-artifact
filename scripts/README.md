# Checker Scripts

`.py` files under `scripts/` helps automating process of experiment.

Specifically, 
- [`gen_histories`](gen_history.py) helps generate a set of histories under particular directory.
- [`reproduce`](reproduce.py) helps generate `.csv` output files of one pass of experiment.
- [`test-multithread`](test-multithread.py) helps run checker on all histories in a particular director.
- [`test-case-props`](test-case-props.py) and [`test`](test.py) is deprecated.
- `.py` files under `samples/` are useful samples when exploring dependencies.

## Dependencies

```sh
pip3 install humanize psutil rich pandas pytest-shutil prettytable
```

see [`requirements`](requirements2.txt) for a clonable valid environment.