#include "git_utils.hpp"
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <cstdio>
#include <memory>
#include <array>
#include <vector>
#include <cstring>
#include <unistd.h>
#include <sys/wait.h>
#include <git2.h>

bool GitUtils::is_git_repo() {
    git_libgit2_init();
    git_buf buf = {0};
    int error = git_repository_discover(&buf, ".", 0, NULL);
    if (error == 0) {
        git_buf_dispose(&buf);
        return true;
    }
    git_buf_dispose(&buf);
    return false;
}

std::string GitUtils::get_repo_root() {
    git_libgit2_init();
    git_repository *repo = nullptr;
    int error = git_repository_open(&repo, ".");
    if (error != 0) {
        return "";
    }
    const char *workdir = git_repository_workdir(repo);
    std::string result = workdir ? workdir : "";
    git_repository_free(repo);
    return result;
}

std::string GitUtils::get_diff(bool cached) {
    git_libgit2_init();
    git_repository *repo = nullptr;
    int error = git_repository_open(&repo, ".");
    if (error != 0) {
        throw std::runtime_error("Failed to open repository");
    }

    git_diff *diff = nullptr;
    if (cached) {
        // index vs HEAD
        git_index *index = nullptr;
        git_repository_index(&index, repo);
        git_tree *head_tree = nullptr;
        git_reference *head_ref = nullptr;
        git_repository_head(&head_ref, repo);
        git_commit *head_commit = nullptr;
        git_reference_peel((git_object **)&head_commit, head_ref, GIT_OBJECT_COMMIT);
        git_commit_tree(&head_tree, head_commit);
        error = git_diff_tree_to_index(&diff, repo, head_tree, index, nullptr);
        git_index_free(index);
        git_tree_free(head_tree);
        git_commit_free(head_commit);
        git_reference_free(head_ref);
    } else {
        // working dir vs index
        error = git_diff_index_to_workdir(&diff, repo, nullptr, nullptr);
    }
    if (error != 0) {
        git_repository_free(repo);
        throw std::runtime_error("Failed to create diff");
    }

    git_buf buf = {0};
    error = git_diff_to_buf(&buf, diff, GIT_DIFF_FORMAT_PATCH);
    std::string result = buf.ptr ? buf.ptr : "";
    git_buf_dispose(&buf);
    git_diff_free(diff);
    git_repository_free(repo);
    return result;
}

std::string GitUtils::get_full_diff() {
    git_libgit2_init();
    git_repository *repo = nullptr;
    int error = git_repository_open(&repo, ".");
    if (error != 0) {
        throw std::runtime_error("Failed to open repository");
    }

    git_tree *head_tree = nullptr;
    git_reference *head_ref = nullptr;
    git_repository_head(&head_ref, repo);
    git_commit *head_commit = nullptr;
    git_reference_peel((git_object **)&head_commit, head_ref, GIT_OBJECT_COMMIT);
    git_commit_tree(&head_tree, head_commit);

    git_diff *diff = nullptr;
    error = git_diff_tree_to_workdir(&diff, repo, head_tree, nullptr);
    if (error != 0) {
        git_tree_free(head_tree);
        git_commit_free(head_commit);
        git_reference_free(head_ref);
        git_repository_free(repo);
        throw std::runtime_error("Failed to create diff");
    }

    git_buf buf = {0};
    error = git_diff_to_buf(&buf, diff, GIT_DIFF_FORMAT_PATCH);
    std::string result = buf.ptr ? buf.ptr : "";
    git_buf_dispose(&buf);
    git_diff_free(diff);
    git_tree_free(head_tree);
    git_commit_free(head_commit);
    git_reference_free(head_ref);
    git_repository_free(repo);
    return result;
}

std::vector<std::string> GitUtils::get_unstaged_files() {
    git_libgit2_init();
    git_repository *repo = nullptr;
    int error = git_repository_open(&repo, ".");
    if (error != 0) {
        throw std::runtime_error("Failed to open repository");
    }

    git_status_list *status_list = nullptr;
    error = git_status_list_new(&status_list, repo, nullptr);
    if (error != 0) {
        git_repository_free(repo);
        throw std::runtime_error("Failed to get status");
    }

    std::vector<std::string> files;
    size_t count = git_status_list_entrycount(status_list);
    for (size_t i = 0; i < count; ++i) {
        const git_status_entry *entry = git_status_byindex(status_list, i);
        if (entry->status & GIT_STATUS_WT_MODIFIED) {
            files.push_back(entry->index_to_workdir->new_file.path);
        }
    }
    git_status_list_free(status_list);
    git_repository_free(repo);
    return files;
}

std::vector<std::string> GitUtils::get_tracked_modified_files() {
    git_libgit2_init();
    git_repository *repo = nullptr;
    int error = git_repository_open(&repo, ".");
    if (error != 0) {
        throw std::runtime_error("Failed to open repository");
    }

    git_status_list *status_list = nullptr;
    error = git_status_list_new(&status_list, repo, nullptr);
    if (error != 0) {
        git_repository_free(repo);
        throw std::runtime_error("Failed to get status");
    }

    std::vector<std::string> files;
    size_t count = git_status_list_entrycount(status_list);
    for (size_t i = 0; i < count; ++i) {
        const git_status_entry *entry = git_status_byindex(status_list, i);
        if (entry->status & GIT_STATUS_INDEX_MODIFIED) {
            files.push_back(entry->head_to_index->new_file.path);
        }
    }
    git_status_list_free(status_list);
    git_repository_free(repo);
    return files;
}

std::vector<std::string> GitUtils::get_untracked_files() {
    git_libgit2_init();
    git_repository *repo = nullptr;
    int error = git_repository_open(&repo, ".");
    if (error != 0) {
        throw std::runtime_error("Failed to open repository");
    }

    git_status_list *status_list = nullptr;
    error = git_status_list_new(&status_list, repo, nullptr);
    if (error != 0) {
        git_repository_free(repo);
        throw std::runtime_error("Failed to get status");
    }

    std::vector<std::string> files;
    size_t count = git_status_list_entrycount(status_list);
    for (size_t i = 0; i < count; ++i) {
        const git_status_entry *entry = git_status_byindex(status_list, i);
        if (entry->status & GIT_STATUS_WT_NEW) {
            files.push_back(entry->index_to_workdir->new_file.path);
        }
    }
    git_status_list_free(status_list);
    git_repository_free(repo);
    return files;
}

void GitUtils::add_files() {
    git_libgit2_init();
    git_repository *repo = nullptr;
    int error = git_repository_open(&repo, ".");
    if (error != 0) {
        throw std::runtime_error("Failed to open repository");
    }

    git_index *index = nullptr;
    git_repository_index(&index, repo);
    error = git_index_add_all(index, nullptr, 0, nullptr, nullptr);
    if (error != 0) {
        git_index_free(index);
        git_repository_free(repo);
        throw std::runtime_error("Failed to add files");
    }
    git_index_write(index);
    git_index_free(index);
    git_repository_free(repo);
}

void GitUtils::add_files(const std::vector<std::string>& files) {
    if (files.empty()) return;
    git_libgit2_init();
    git_repository *repo = nullptr;
    int error = git_repository_open(&repo, ".");
    if (error != 0) {
        throw std::runtime_error("Failed to open repository");
    }

    git_index *index = nullptr;
    git_repository_index(&index, repo);
    for (const auto& file : files) {
        error = git_index_add_bypath(index, file.c_str());
        if (error != 0) {
            git_index_free(index);
            git_repository_free(repo);
            throw std::runtime_error("Failed to add file: " + file);
        }
    }
    git_index_write(index);
    git_index_free(index);
    git_repository_free(repo);
}

void GitUtils::commit(const std::string& message) {
    git_libgit2_init();
    git_repository *repo = nullptr;
    int error = git_repository_open(&repo, ".");
    if (error != 0) {
        throw std::runtime_error("Failed to open repository");
    }

    git_index *index = nullptr;
    git_repository_index(&index, repo);
    git_oid tree_oid;
    git_index_write_tree(&tree_oid, index);
    git_tree *tree = nullptr;
    git_tree_lookup(&tree, repo, &tree_oid);
    git_index_free(index);

    git_reference *head_ref = nullptr;
    git_repository_head(&head_ref, repo);
    git_commit *parent_commit = nullptr;
    git_reference_peel((git_object **)&parent_commit, head_ref, GIT_OBJECT_COMMIT);
    const git_commit *parents[] = {parent_commit};

    git_signature *author = nullptr;
    git_signature_default(&author, repo);

    git_oid commit_oid;
    error = git_commit_create(&commit_oid, repo, "HEAD", author, author, "UTF-8", message.c_str(), tree, 1, parents);
    if (error != 0) {
        git_signature_free(author);
        git_commit_free(parent_commit);
        git_reference_free(head_ref);
        git_tree_free(tree);
        git_repository_free(repo);
        throw std::runtime_error("Git commit failed");
    }

    git_signature_free(author);
    git_commit_free(parent_commit);
    git_reference_free(head_ref);
    git_tree_free(tree);
    git_repository_free(repo);
}

std::pair<std::string, std::string> GitUtils::commit_with_output(const std::string& message) {
    git_libgit2_init();
    git_repository *repo = nullptr;
    int error = git_repository_open(&repo, ".");
    if (error != 0) {
        throw std::runtime_error("Failed to open repository");
    }

    git_index *index = nullptr;
    git_repository_index(&index, repo);
    git_oid tree_oid;
    git_index_write_tree(&tree_oid, index);
    git_tree *tree = nullptr;
    git_tree_lookup(&tree, repo, &tree_oid);
    git_index_free(index);

    git_reference *head_ref = nullptr;
    git_repository_head(&head_ref, repo);
    git_commit *parent_commit = nullptr;
    git_reference_peel((git_object **)&parent_commit, head_ref, GIT_OBJECT_COMMIT);
    const git_commit *parents[] = {parent_commit};

    git_signature *author = nullptr;
    git_signature_default(&author, repo);

    git_oid commit_oid;
    error = git_commit_create(&commit_oid, repo, "HEAD", author, author, "UTF-8", message.c_str(), tree, 1, parents);
    if (error != 0) {
        git_signature_free(author);
        git_commit_free(parent_commit);
        git_reference_free(head_ref);
        git_tree_free(tree);
        git_repository_free(repo);
        throw std::runtime_error("Git commit failed");
    }

    char hash_str[GIT_OID_HEXSZ + 1];
    git_oid_tostr(hash_str, sizeof(hash_str), &commit_oid);
    std::string hash = hash_str;
    std::string output = "[" + hash + "] " + message;

    git_signature_free(author);
    git_commit_free(parent_commit);
    git_reference_free(head_ref);
    git_tree_free(tree);
    git_repository_free(repo);
    return {hash, output};
}

struct progress_data {
    int pack_total;
    int pack_current;
    int transfer_total;
    int transfer_current;
};

int pack_progress_cb(int stage, uint32_t current, uint32_t total, void *payload) {
    progress_data *pd = (progress_data*)payload;
    pd->pack_current = current;
    pd->pack_total = total;
    std::cout << "\rPacking: " << current << "/" << total << std::flush;
    return 0;
}

int transfer_progress_cb(const git_transfer_progress *stats, void *payload) {
    progress_data *pd = (progress_data*)payload;
    pd->transfer_current = stats->received_objects;
    pd->transfer_total = stats->total_objects;
    std::cout << "\rTransferring: " << stats->received_objects << "/" << stats->total_objects << std::flush;
    return 0;
}

void GitUtils::push() {
    git_libgit2_init();
    git_repository *repo = nullptr;
    int error = git_repository_open(&repo, ".");
    if (error != 0) {
        throw std::runtime_error("Failed to open repository");
    }

    // Find remote "origin"
    git_remote *remote = nullptr;
    error = git_remote_lookup(&remote, repo, "origin");
    if (error != 0) {
        git_repository_free(repo);
        const git_error *err = git_error_last();
        std::string msg = "No 'origin' remote found";
        if (err) msg += ": " + std::string(err->message);
        msg += "\nSuggestion: Add a remote with 'git remote add origin <url>'";
        throw std::runtime_error(msg);
    }

    // Get remote URL for diagnostics
    const char *remote_url = git_remote_url(remote);

    // Get current branch
    git_reference *head_ref = nullptr;
    git_repository_head(&head_ref, repo);
    const char *branch_name = git_reference_shorthand(head_ref);

    // Check if branch has upstream
    git_reference *upstream_ref = nullptr;
    error = git_branch_upstream(&upstream_ref, head_ref);
    if (error != 0) {
        git_reference_free(head_ref);
        git_remote_free(remote);
        git_repository_free(repo);
        std::string msg = "Current branch '" + std::string(branch_name) + "' has no upstream tracking branch";
        msg += "\nSuggestion: Set upstream with 'git branch --set-upstream-to=origin/" + std::string(branch_name) + "'";
        throw std::runtime_error(msg);
    }
    git_reference_free(upstream_ref);

    // Push options
    git_push_options push_opts = GIT_PUSH_OPTIONS_INIT;
    progress_data pd = {0, 0, 0, 0};
    push_opts.callbacks.pack_progress = pack_progress_cb;
    push_opts.callbacks.transfer_progress = transfer_progress_cb;
    push_opts.callbacks.payload = &pd;

    // Refspecs
    git_strarray refspecs = {0};
    std::string refspec = std::string("refs/heads/") + branch_name + ":refs/heads/" + branch_name;
    char *refspec_ptr = strdup(refspec.c_str());
    refspecs.strings = &refspec_ptr;
    refspecs.count = 1;

    error = git_remote_push(remote, &refspecs, &push_opts);

    free(refspec_ptr);
    git_reference_free(head_ref);
    git_remote_free(remote);
    git_repository_free(repo);

    std::cout << std::endl;

    if (error != 0) {
        const git_error *err = git_error_last();
        std::string msg = "Push failed";
        if (err) {
            msg += ": " + std::string(err->message);
        }
        msg += "\nRemote: " + std::string(remote_url ? remote_url : "unknown");
        msg += "\nBranch: " + std::string(branch_name);
        // Add suggestions based on common errors
        if (err && std::string(err->message).find("authentication") != std::string::npos) {
            msg += "\nSuggestion: Check your credentials or SSH key configuration";
        } else if (err && std::string(err->message).find("network") != std::string::npos) {
            msg += "\nSuggestion: Verify internet connection and remote URL";
        } else {
            msg += "\nSuggestion: Ensure you have push permissions and the remote is accessible";
        }
        throw std::runtime_error(msg);
    }
}