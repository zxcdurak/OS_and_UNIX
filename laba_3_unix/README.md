# Жёсткие ссылки: дедупликация файлов

Консольная программа на C: обходит каталог, считает SHA-1 каждого обычного файла
(OpenSSL EVP) и заменяет дубликаты жёсткими ссылками на один оставленный экземпляр.
Замена безопасная: `link()` во временный файл `имя.linkstemp`, затем атомарный `rename()`.

## Сборка

```bash
sudo apt install libssl-dev
gcc -O2 -Wall -Wextra -o hardlinks hardlinks.c -lcrypto
```

## Структура `test_dir`

```
test_dir/
├── a.txt                      ┐
├── b.txt                      │ 5 одинаковых файлов
├── sub/c.txt                  │
├── sub/deep/d.txt             │
├── dir with spaces/           │
│   └── file with spaces.txt   ┘
├── unique.txt                 уникальное содержимое
├── same_size_other.txt        тот же размер, что у a.txt, другое содержимое
├── other/
│   ├── big1.bin               ┐ 3 одинаковых файла по 5 МБ
│   ├── big2.bin               │
│   └── big_modified.bin       отличается от big1.bin последним байтом
├── sub/big3.bin               ┘ копия big1.bin
├── empty1                     ┐ два пустых файла
├── sub/empty2                 ┘
├── linked_a.txt               ┐ уже жёсткие ссылки друг на друга
├── linked_b.txt               ┘
├── linked_copy.txt            независимая копия с тем же содержимым
├── symlink_to_a.txt  -> a.txt символьная ссылка
├── symlink_to_dir    -> sub   символьная ссылка на каталог
└── my_fifo                    именованный канал
```

## Запуск

```bash
./hardlinks test_dir
```

## Результат

```
dbc6f891ed1aa830aed20ccfa923cc10ca6eb0ab  test_dir/same_size_other.txt
25da4a614d79e781259cc2273814e3349a7edaa2  test_dir/other/big2.bin
82acc737185aa298d94d9110d41a21d023a42bb4  test_dir/other/big_modified.bin
25da4a614d79e781259cc2273814e3349a7edaa2  test_dir/other/big1.bin
da39a3ee5e6b4b0d3255bfef95601890afd80709  test_dir/empty1
22596363b3de40b06f981fb85d82312e8c0ed511  test_dir/b.txt
7abef72ae41f1e9ea6f480ec9f48a1f6e59fc559  test_dir/linked_b.txt
7abef72ae41f1e9ea6f480ec9f48a1f6e59fc559  test_dir/linked_a.txt
22596363b3de40b06f981fb85d82312e8c0ed511  test_dir/a.txt
7abef72ae41f1e9ea6f480ec9f48a1f6e59fc559  test_dir/linked_copy.txt
22596363b3de40b06f981fb85d82312e8c0ed511  test_dir/sub/deep/d.txt
25da4a614d79e781259cc2273814e3349a7edaa2  test_dir/sub/big3.bin
da39a3ee5e6b4b0d3255bfef95601890afd80709  test_dir/sub/empty2
22596363b3de40b06f981fb85d82312e8c0ed511  test_dir/sub/c.txt
089ca91e56e725986f85f83be240bbf0046059ad  test_dir/unique.txt
22596363b3de40b06f981fb85d82312e8c0ed511  test_dir/dir with spaces/file with spaces.txt
link: test_dir/b.txt -> test_dir/a.txt
link: test_dir/dir with spaces/file with spaces.txt -> test_dir/a.txt
link: test_dir/sub/c.txt -> test_dir/a.txt
link: test_dir/sub/deep/d.txt -> test_dir/a.txt
link: test_dir/other/big2.bin -> test_dir/other/big1.bin
link: test_dir/sub/big3.bin -> test_dir/other/big1.bin
link: test_dir/linked_copy.txt -> test_dir/linked_a.txt
link: test_dir/sub/empty2 -> test_dir/empty1
replaced: 8
```

Разбор результата:

| Группа (одинаковый хэш) | Что произошло |
|---|---|
| `a.txt`, `b.txt`, `sub/c.txt`, `sub/deep/d.txt`, `file with spaces.txt` (`2259…`) | 4 файла заменены ссылками на `a.txt` |
| `big1.bin`, `big2.bin`, `sub/big3.bin` (`25da…`) | 2 файла заменены ссылками на `big1.bin` |
| `empty1`, `sub/empty2` (`da39…`) | `sub/empty2` заменён ссылкой на `empty1` |
| `linked_a.txt`, `linked_b.txt`, `linked_copy.txt` (`7abe…`) | `linked_copy.txt` заменён ссылкой на `linked_a.txt`; `linked_b.txt` уже был ссылкой на `linked_a.txt` и пропущен |
| `unique.txt`, `same_size_other.txt`, `big_modified.bin` | уникальные хэши, не тронуты |
| `symlink_to_a.txt`, `symlink_to_dir`, `my_fifo` | в список не попали, игнорируются |

Итого заменено 8 дубликатов.