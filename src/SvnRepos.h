#pragma once
#include <vector>
#include <filesystem>
#include <memory>

class SvnReposImpl;
class SvnRepos final
{
  // delete copy/assignement operators
  SvnRepos(const SvnRepos&) = delete;
  SvnRepos& operator=(const SvnRepos&) = delete;
  SvnRepos(SvnRepos&&) = delete;
  SvnRepos& operator=(SvnRepos&&) = delete;

public:
  // constructor/destructor
  SvnRepos();
  ~SvnRepos();

  // start all the threads to update svn repositories
  void update(const std::vector<std::filesystem::path>& repos);

  // stop the svn-update process properly (terminates the current running tasks)
  void stop();

private:
  // pointer to internal implementation
  std::unique_ptr<SvnReposImpl> m_pimpl;
};