// Copyright 2024 The DriverHub Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! Topological sort for module dependency ordering.
//!
//! Uses Kahn's algorithm to produce a load order where each module
//! appears after all of its dependencies.

use std::collections::{HashMap, VecDeque};

use crate::modinfo::ModuleInfo;

/// Topologically sort modules by their dependency relationships.
///
/// Reads the `depends` field from each module's `ModuleInfo` to build a
/// dependency graph. Returns `Some(sorted_indices)` on success, or `None`
/// if a dependency cycle is detected.
pub fn topological_sort_modules(
    modules: &[(String, ModuleInfo)],
) -> Option<Vec<usize>> {
    let name_to_idx: HashMap<&str, usize> = modules
        .iter()
        .enumerate()
        .map(|(i, (name, _))| (name.as_str(), i))
        .collect();

    let n = modules.len();
    let mut dependents: Vec<Vec<usize>> = vec![Vec::new(); n];
    let mut in_degree: Vec<usize> = vec![0; n];

    for (i, (_, info)) in modules.iter().enumerate() {
        for dep in &info.depends {
            if let Some(&dep_idx) = name_to_idx.get(dep.as_str()) {
                dependents[dep_idx].push(i);
                in_degree[i] += 1;
            } else {
                eprintln!(
                    "driverhub: dependency '{}' of '{}' not in module set \
                     (may already be loaded)",
                    dep, modules[i].0
                );
            }
        }
    }

    // Kahn's algorithm.
    let mut ready: VecDeque<usize> = (0..n).filter(|&i| in_degree[i] == 0).collect();
    let mut sorted = Vec::with_capacity(n);

    while let Some(idx) = ready.pop_front() {
        sorted.push(idx);
        for &dep_idx in &dependents[idx] {
            in_degree[dep_idx] -= 1;
            if in_degree[dep_idx] == 0 {
                ready.push_back(dep_idx);
            }
        }
    }

    if sorted.len() != n {
        eprintln!(
            "driverhub: dependency cycle detected! Sorted {} of {} modules.",
            sorted.len(),
            n
        );
        None
    } else {
        Some(sorted)
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn make_info(name: &str, depends: &[&str]) -> (String, ModuleInfo) {
        (
            name.to_string(),
            ModuleInfo {
                name: name.to_string(),
                depends: depends.iter().map(|s| s.to_string()).collect(),
                ..Default::default()
            },
        )
    }

    fn find_position(order: &[usize], idx: usize) -> usize {
        order.iter().position(|&i| i == idx).expect("index not found")
    }

    #[test]
    fn no_dependencies() {
        let modules = vec![make_info("a", &[]), make_info("b", &[]), make_info("c", &[])];
        let order = topological_sort_modules(&modules).unwrap();
        assert_eq!(order.len(), 3);
    }

    #[test]
    fn linear_chain() {
        let modules = vec![
            make_info("a", &[]),
            make_info("b", &["a"]),
            make_info("c", &["b"]),
        ];
        let order = topological_sort_modules(&modules).unwrap();
        assert_eq!(order.len(), 3);
        assert!(find_position(&order, 0) < find_position(&order, 1));
        assert!(find_position(&order, 1) < find_position(&order, 2));
    }

    #[test]
    fn diamond_dependency() {
        let modules = vec![
            make_info("a", &[]),
            make_info("b", &["a"]),
            make_info("c", &["a"]),
            make_info("d", &["b", "c"]),
        ];
        let order = topological_sort_modules(&modules).unwrap();
        assert_eq!(order.len(), 4);
        assert!(find_position(&order, 0) < find_position(&order, 1));
        assert!(find_position(&order, 0) < find_position(&order, 2));
        assert!(find_position(&order, 1) < find_position(&order, 3));
        assert!(find_position(&order, 2) < find_position(&order, 3));
    }

    #[test]
    fn cycle_detection() {
        let modules = vec![make_info("a", &["b"]), make_info("b", &["a"])];
        assert!(topological_sort_modules(&modules).is_none());
    }

    #[test]
    fn external_dependency_skipped() {
        let modules = vec![make_info("a", &["external_lib"])];
        let order = topological_sort_modules(&modules).unwrap();
        assert_eq!(order.len(), 1);
    }

    #[test]
    fn empty_module_list() {
        let modules: Vec<(String, ModuleInfo)> = vec![];
        let order = topological_sort_modules(&modules).unwrap();
        assert!(order.is_empty());
    }

    #[test]
    fn self_dependency() {
        let modules = vec![make_info("a", &["a"])];
        assert!(topological_sort_modules(&modules).is_none());
    }

    #[test]
    fn long_chain() {
        let modules: Vec<_> = (0..10)
            .map(|i| {
                let deps = if i > 0 {
                    vec![format!("m{}", i - 1)]
                } else {
                    vec![]
                };
                (
                    format!("m{}", i),
                    ModuleInfo {
                        name: format!("m{}", i),
                        depends: deps,
                        ..Default::default()
                    },
                )
            })
            .collect();
        let order = topological_sort_modules(&modules).unwrap();
        assert_eq!(order.len(), 10);
        for i in 1..10 {
            assert!(
                find_position(&order, i - 1) < find_position(&order, i),
                "m{} should appear before m{}",
                i - 1,
                i
            );
        }
    }
}
