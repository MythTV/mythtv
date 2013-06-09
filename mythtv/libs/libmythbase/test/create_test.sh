#!/bin/bash -e

print_help() {
    echo "Enter name of component to test (no spaces)"
}

if [[ "$1" == "" ]] ; then
  print_help
  exit 1
fi

echo "Creating test_$1"
mkdir test_$1

for EXT in pro cpp h ; do
  echo "Creating test_$1/test_$1.$EXT"
  cp test_template/test_template.$EXT test_$1/test_$1.$EXT
  sed -i test_$1/test_$1.$EXT -e "s/test_template/test_$1/g"
  sed -i test_$1/test_$1.$EXT -e "s/TestTemplate/Test$1/g"
  git add test_$1/test_$1.$EXT
done

cat > test_$1/.gitignore <<EOF
test_$1
*.gcda
*.gcno
*.gcov
EOF
git add test_$1/.gitignore

echo git commit -m "Add test template for $1"
