#include <queue>
#include <map>
#include <stdbool.h>
#include <atomic>
#include <thread>
#include <mutex>
#include <regex>
#include <fort.hpp>
#include <spdlog/spdlog.h>
#include <winpp/progress-bar.hpp>
#include <winpp/win.hpp>
#include <winpp/utf8.hpp>
#include "SvnRepos.h"

// separate implementation from the interface
class SvnReposImpl final
{
public:
  // constructor
  SvnReposImpl() :
    m_repos(),
    m_results(),
    m_progress_bar(),
    m_nb_repos(),
    m_running(false),
    m_threads(),
    m_mutex()
  {
  }

  // destructor
  ~SvnReposImpl()
  {
    // stop threads
    m_running = false;
    for (auto& t : m_threads)
      if (t.joinable())
        t.join();
  }

  // start all the threads to update svn repositories
  void update(const std::vector<std::filesystem::path>& repos)
  {
    // initialize update variables
    m_repos = std::queue(std::deque(repos.begin(), repos.end()));
    m_nb_repos = m_repos.size();
    m_results.clear();

    {
      // create progress-bar
      m_progress_bar = std::make_unique<console::progress_bar>("update svn repositories: ", m_nb_repos);

      // start threads
      spdlog::debug(fmt::format(fmt::emphasis::bold, "launch the svn update commands on repositories:\n"));
      m_running = true;
      const std::size_t max_cpu = static_cast<std::size_t>(std::thread::hardware_concurrency());
      m_threads = std::vector<std::thread>(std::min(m_nb_repos, max_cpu));
      for (auto& t : m_threads)
        t = std::thread(&SvnReposImpl::run, this);

      // wait for threads completion
      for (auto& t : m_threads)
        if (t.joinable())
          t.join();

      // delete progress-bar
      m_progress_bar.reset();
    }

    // log updated repositories as table
    log();
  }

  // stop the svn-update process properly (terminates the current running tasks)
  void stop()
  {
    m_running = false;
  }

private:
  // run the update process - thread
  void run()
  {
    while (m_running)
    {
      // retrieve one repos - protected by mutex
      std::filesystem::path repo;
      {
        std::lock_guard<std::mutex> lck(m_mutex);
        if (m_repos.empty())
          return;
        repo = m_repos.front().parent_path();
        m_repos.pop();
      }

      // execute the update process
      win::sync_process proc;
      proc.set_working_dir(repo);
      proc.set_timeout(std::chrono::milliseconds(60000));
      const int exit_code = proc.execute("svn.exe update --accept theirs-full");

      // store result and update progress-bar - protected by mutex
      {
        std::lock_guard<std::mutex> lck(m_mutex);
        std::regex update_ok(R"(^A |^D |^U |^C |^G |E^ |R^ )");
        if (exit_code != 0)
          m_results[repo] = false;
        else if (std::regex_search(proc.get_logs(), update_ok))
          m_results[repo] = true;
        m_progress_bar->tick();
      }
    }
  }

  // log the update process result as an ascii table
  void log()
  {
    if (!m_results.empty())
    {
      // create table stylesheet
      fort::utf8_table table;
      table.set_border_style(FT_NICE_STYLE);
      table.column(0).set_cell_text_align(fort::text_align::left);
      table.column(0).set_cell_content_text_style(fort::text_style::bold);
      table.column(1).set_cell_text_align(fort::text_align::center);
      table.column(1).set_cell_content_text_style(fort::text_style::bold);

      // create header
      table << fort::header << "PROJECTS" << "UPDATED" << fort::endr;

      // add rows
      std::size_t idx = 1;
      for (const auto& r : m_results)
      {
        table << utf8::to_utf8(r.first.string()) << (r.second ? "OK" : "KO") << fort::endr;
        table[idx++][1].set_cell_content_fg_color(r.second ? fort::color::green : fort::color::red);
      }
      spdlog::info("{}\n\n", table.to_string());
    }
    spdlog::info("total repositories updated: [{}/{}]\n", m_results.size(), m_nb_repos);
  }

private:
  // svn repos informations
  std::queue<std::filesystem::path> m_repos;
  std::map<std::filesystem::path, bool> m_results;

  // progress-bar
  std::unique_ptr<console::progress_bar> m_progress_bar;
  std::size_t m_nb_repos;

  // handling threads
  std::atomic<bool> m_running;
  std::vector<std::thread> m_threads;
  std::mutex m_mutex;
};

// constructor
SvnRepos::SvnRepos() :
  m_pimpl(std::make_unique<SvnReposImpl>())
{
}

// destructor
SvnRepos::~SvnRepos() = default;

// start all the threads to update svn repositories
void SvnRepos::update(const std::vector<std::filesystem::path>& repos)
{
  if (m_pimpl)
    m_pimpl->update(repos);
}

// stop the svn-update process properly (terminates the current running tasks)
void SvnRepos::stop()
{
  if (m_pimpl)
    m_pimpl->stop();
}